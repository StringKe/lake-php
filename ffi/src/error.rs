use lake_client::Error as ApiError;
use lake_driver::Error;

pub const GENERAL: &str = "HY000";
pub const CONNECT_FAILED: &str = "08001";
pub const LINK_FAILURE: &str = "08S01";
pub const AUTH_FAILED: &str = "28000";
pub const INVALID_PARAM: &str = "HY093";
pub const INVALID_TEXT: &str = "22021";

pub struct Failure {
    pub sqlstate: &'static str,
    pub code: i64,
    pub message: String,
}

impl Failure {
    pub fn new(sqlstate: &'static str, message: impl Into<String>) -> Self {
        Self { sqlstate, code: 0, message: message.into() }
    }

    pub fn from_driver(error: &Error, connecting: bool) -> Self {
        let link = if connecting { CONNECT_FAILED } else { LINK_FAILURE };
        match error {
            Error::Api(api) => classify_api(api, link).unwrap_or_else(|| Self::new(GENERAL, text(error))),
            Error::Transport(_) => Self::new(link, text(error)),
            _ => Self::new(GENERAL, text(error)),
        }
    }
}

/// The driver's Display appends its own version tag, which is noise in a PDO error message.
fn text(error: &impl ToString) -> String {
    let mut message = error.to_string();
    if message.ends_with(']')
        && let Some(start) = message.rfind(" [v")
    {
        message.truncate(start);
    }
    without_request_dump(&message).to_string()
}

/// The TiDB Cloud Lake gateway appends the whole HTTP request to some errors, session cookie and internal addresses included.
const REQUEST_DUMP: &str = ", request=Request {";

fn without_request_dump(message: &str) -> &str {
    message.find(REQUEST_DUMP).map_or(message, |start| &message[..start])
}

fn server_message(error: &ApiError) -> Option<&str> {
    match error {
        ApiError::WithContext { inner, .. } => server_message(inner),
        ApiError::QueryFailed(ec) | ApiError::Logic(_, ec) => Some(&ec.message),
        _ => None,
    }
}

/// After the last page the client fetches the query's `/final` URI. When the gateway no longer finds the node that ran the query,
/// that request fails although every row has already been delivered, and the server drops the query on its own.
pub fn is_lost_after_last_page(error: &Error) -> bool {
    match error {
        Error::Api(api) => server_message(api).is_some_and(lost_on_final_request),
        _ => false,
    }
}

fn lost_on_final_request(message: &str) -> bool {
    let Some(start) = message.find(REQUEST_DUMP) else { return false };
    let request = &message[start..];
    let uri = request.find("uri: ").map(|at| &request[at + 5..]).and_then(|rest| rest.split([',', ' ']).next());
    message[..start].contains("Sticky session state lost") && uri.is_some_and(|uri| uri.ends_with("/final"))
}

/// Server error codes observed on Databend 1.2.881; unlisted codes stay HY000 with the native code kept.
fn query_sqlstate(code: u16) -> &'static str {
    match code {
        1005 | 1008 | 1063 | 1065 => "42000",
        1025 => "42S02",
        2302 => "42S01",
        1003 => "3D000",
        1006 | 1046 => "22000",
        1303 => "21S01",
        _ => GENERAL,
    }
}

fn server_error(sqlstate: &'static str, code: u16, message: &str, detail: Option<&str>) -> Failure {
    let message = without_request_dump(message);
    let message = match detail {
        Some(detail) if !detail.is_empty() => format!("{}\n{detail}", message.trim_end()),
        _ => message.trim_end().to_string(),
    };
    Failure { sqlstate, code: i64::from(code), message }
}

fn classify_api(error: &ApiError, link: &'static str) -> Option<Failure> {
    let status_failure = |sqlstate, status: u16| Failure { sqlstate, code: i64::from(status), message: text(error) };
    match error {
        ApiError::WithContext { inner, .. } => classify_api(inner, link),
        ApiError::QueryFailed(ec) | ApiError::Logic(_, ec) => Some(server_error(query_sqlstate(ec.code), ec.code, &ec.message, ec.detail.as_deref())),
        ApiError::AuthFailure(ec) => Some(server_error(AUTH_FAILED, ec.code, &ec.message, ec.detail.as_deref())),
        ApiError::Request(_) => Some(Failure::new(link, text(error))),
        ApiError::Response { status, .. } if status.as_u16() == 401 || status.as_u16() == 403 => Some(status_failure(AUTH_FAILED, status.as_u16())),
        ApiError::Response { status, .. } if status.is_server_error() => Some(status_failure(link, status.as_u16())),
        ApiError::Response { status, .. } => Some(status_failure(GENERAL, status.as_u16())),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const LOST_ON_FINAL: &str = r#"Some(400) BadArguments. Code: 1006, Text = Sticky session state lost: node 'zlo1JshXNoSa2p0b43idU1' not found in cluster, request=Request { method: GET, uri: /query/01a1b4f56f00771aa494f3121531c051/final, version: HTTP/1.1, headers: {"cookie": "session_id=01a1b4f5-5b85-73d3-80e6-52269948d0cd", "x-databend-sticky-node": "zlo1JshXNoSa2p0b43idU1"} }"#;

    #[test]
    fn sticky_node_lost_on_the_final_request_ends_the_result() {
        assert!(lost_on_final_request(LOST_ON_FINAL));
    }

    #[test]
    fn sticky_node_lost_while_pages_remain_is_an_error() {
        let on_page = LOST_ON_FINAL.replace("/final,", "/page/2,");
        assert!(!lost_on_final_request(&on_page));
    }

    #[test]
    fn other_errors_on_the_final_request_are_errors() {
        let other = LOST_ON_FINAL.replace("Sticky session state lost", "Query not found");
        assert!(!lost_on_final_request(&other));
        assert!(!lost_on_final_request("Sticky session state lost: node 'x' not found in cluster"));
    }

    #[test]
    fn request_dump_is_cut_from_server_messages() {
        let failure = server_error(GENERAL, 400, LOST_ON_FINAL, None);
        assert_eq!(failure.message, "Some(400) BadArguments. Code: 1006, Text = Sticky session state lost: node 'zlo1JshXNoSa2p0b43idU1' not found in cluster");
        assert_eq!(without_request_dump("1025 Unknown table"), "1025 Unknown table");
    }
}
