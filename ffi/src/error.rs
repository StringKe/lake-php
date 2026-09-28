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
    message
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
