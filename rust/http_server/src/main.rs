use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use tiny_http::{Server, Response, Header, StatusCode};
use mime_guess::from_path;

fn main() {
    let args: Vec<String> = env::args().collect();
    let port = args.get(1).unwrap_or(&"8080".to_string()).to_string();
    let dir = args.get(2).unwrap_or(&".".to_string()).to_string();

    let addr = format!("0.0.0.0:{}", port);
    let server = Server::http(&addr).unwrap();
    println!("🚀 Serving '{}' on http://{}\n", dir, addr);

    for request in server.incoming_requests() {
        handle_request(request, &dir);
    }
}

fn handle_request(request: tiny_http::Request, base_dir: &str) {
    let url_path = request.url().trim_start_matches('/');
    let mut path = PathBuf::from(base_dir);
    path.push(if url_path.is_empty() { "index.html" } else { url_path });

    if path.is_dir() {
        match generate_directory_listing(&path, url_path) {
            Ok(html) => {
                let header = Header::from_bytes("Content-Type", "text/html").unwrap();
                let response = Response::from_string(html).with_header(header);
                request.respond(response).unwrap();
            }
            Err(_) => {
                let response =
                    Response::from_string("500 Internal Server Error").with_status_code(500);
                request.respond(response).unwrap();
            }
        }
        return;
    }

    match fs::read(&path) {
        Ok(contents) => {
            let mime_type = from_path(&path)
                .first_or_text_plain()
                .to_string();

            let header = Header::from_bytes("Content-Type", mime_type).unwrap();
            let response = Response::from_data(contents).with_header(header);
            request.respond(response).unwrap();
        }
        Err(_) => {
            let response = Response::from_string("404 Not Found").with_status_code(StatusCode(404));
            request.respond(response).unwrap();
        }
    }
}

fn generate_directory_listing(path: &Path, url_path: &str) -> std::io::Result<String> {
    let mut html = String::new();
    html.push_str(&format!(
        "<html><head><title>Index of /{}</title></head><body>",
        url_path
    ));
    html.push_str(&format!("<h2>Index of /{}</h2><ul>", url_path));

    for entry in fs::read_dir(path)? {
        let entry = entry?;
        let name = entry.file_name().to_string_lossy().to_string();
        let link = if url_path.is_empty() {
            name.clone()
        } else {
            format!("{}/{}", url_path, name)
        };
        html.push_str(&format!("<li><a href=\"{}\">{}</a></li>", link, name));
    }

    html.push_str("</ul></body></html>");
    Ok(html)
}
