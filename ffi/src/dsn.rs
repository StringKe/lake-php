use url::Url;

const URL_SCHEMES: [&str; 10] = ["lake", "lake+http", "lake+https", "lake+flight", "lake+grpc", "databend", "databend+http", "databend+https", "databend+flight", "databend+grpc"];

pub struct Credentials<'a> {
    pub user: Option<&'a str>,
    pub password: Option<&'a str>,
}

/// Turns the part of a PDO DSN after `lake:` into the URL the official driver accepts.
///
/// Two forms are accepted: a full driver URL (`lake://...`, `lake+flight://...`) and PDO style
/// `key=value;key=value` pairs, where `host`, `port`, `dbname`, `user`, `password` and `protocol`
/// build the URL and every other key is forwarded as a query parameter.
pub fn build_url(data_source: &str, credentials: Credentials<'_>, timeout_secs: i64) -> Result<String, String> {
    let trimmed = data_source.trim();
    let mut url = if is_driver_url(trimmed) { Url::parse(trimmed).map_err(|e| format!("invalid DSN URL: {e}"))? } else { from_pairs(trimmed)? };

    if let Some(user) = credentials.user.filter(|u| !u.is_empty()) {
        url.set_username(user).map_err(|_| "DSN host does not accept a user name".to_string())?;
    }
    if let Some(password) = credentials.password {
        url.set_password(Some(password)).map_err(|_| "DSN host does not accept a password".to_string())?;
    }
    if timeout_secs > 0 && !url.query_pairs().any(|(k, _)| k == "connect_timeout") {
        url.query_pairs_mut().append_pair("connect_timeout", &timeout_secs.to_string());
    }
    Ok(url.into())
}

fn is_driver_url(value: &str) -> bool {
    value.split_once("://").is_some_and(|(scheme, _)| URL_SCHEMES.contains(&scheme.to_ascii_lowercase().as_str()))
}

fn from_pairs(data_source: &str) -> Result<Url, String> {
    let mut host = None;
    let mut port = None;
    let mut database = None;
    let mut user = None;
    let mut password = None;
    let mut scheme = "lake";
    let mut extra = Vec::new();

    for part in data_source.split(';').map(str::trim).filter(|p| !p.is_empty()) {
        let (key, value) = part.split_once('=').ok_or_else(|| format!("DSN part `{part}` is not key=value"))?;
        let (key, value) = (key.trim(), value.trim());
        match key.to_ascii_lowercase().as_str() {
            "host" => host = Some(value),
            "port" => port = Some(value.parse::<u16>().map_err(|_| format!("DSN port `{value}` is not a valid port"))?),
            "dbname" | "database" => database = Some(value),
            "user" => user = Some(value),
            "password" => password = Some(value),
            "protocol" => {
                scheme = match value.to_ascii_lowercase().as_str() {
                    "http" | "rest" => "lake",
                    "flight" | "flightsql" | "grpc" => "lake+flight",
                    other => return Err(format!("DSN protocol `{other}` is not one of http, flight")),
                }
            }
            _ => extra.push((key, value)),
        }
    }

    let host = host.filter(|h| !h.is_empty()).ok_or("DSN requires host")?;
    let mut url = Url::parse(&format!("{scheme}://{host}")).map_err(|e| format!("invalid DSN host `{host}`: {e}"))?;
    url.set_port(port).map_err(|_| "DSN host does not accept a port".to_string())?;
    if let Some(user) = user {
        url.set_username(user).map_err(|_| "DSN host does not accept a user name".to_string())?;
    }
    if let Some(password) = password {
        url.set_password(Some(password)).map_err(|_| "DSN host does not accept a password".to_string())?;
    }
    if let Some(database) = database.filter(|d| !d.is_empty()) {
        url.path_segments_mut().map_err(|_| "DSN host cannot carry a database".to_string())?.push(database);
    }
    if !extra.is_empty() {
        let mut query = url.query_pairs_mut();
        for (key, value) in extra {
            query.append_pair(key, value);
        }
    }
    Ok(url)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn none() -> Credentials<'static> {
        Credentials { user: None, password: None }
    }

    #[test]
    fn pairs_build_rest_url_with_passthrough_settings() {
        let url = build_url("host=tn.gw.example.com;port=443;dbname=sales;warehouse=wh;sslmode=enable;max_threads=8", Credentials { user: Some("u@x"), password: Some("p;w&d") }, 0).unwrap();
        assert_eq!(url, "lake://u%40x:p%3Bw&d@tn.gw.example.com:443/sales?warehouse=wh&sslmode=enable&max_threads=8");
    }

    #[test]
    fn protocol_flight_switches_scheme_and_timeout_is_appended_once() {
        assert_eq!(build_url("host=h;protocol=flight", none(), 5).unwrap(), "lake+flight://h?connect_timeout=5");
        assert_eq!(build_url("host=h;connect_timeout=9", none(), 5).unwrap(), "lake://h?connect_timeout=9");
    }

    #[test]
    fn driver_url_is_kept_and_credentials_override() {
        assert_eq!(build_url("lake+flight://a:b@h:8900/db?x=1", Credentials { user: Some("c"), password: Some("d") }, 0).unwrap(), "lake+flight://c:d@h:8900/db?x=1");
    }

    #[test]
    fn invalid_input_is_rejected() {
        assert!(build_url("port=1", none(), 0).is_err());
        assert!(build_url("host=h;protocol=mysql", none(), 0).is_err());
        assert!(build_url("host=h;junk", none(), 0).is_err());
    }
}
