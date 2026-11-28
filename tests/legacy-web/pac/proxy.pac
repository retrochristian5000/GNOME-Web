// Deterministic test fixture. No DNS or network helpers.
function FindProxyForURL(url, host) {
    if (host === "direct.example.invalid") {
        return "DIRECT";
    }
    if (host === "fallback.example.invalid") {
        return "PROXY first.example.invalid:8080; PROXY second.example.invalid:8081; DIRECT";
    }
    if (host === "socks.example.invalid") {
        return "SOCKS5 socks-proxy.example.invalid:1080";
    }
    if (url.substring(0, 6) === "https:") {
        return "PROXY secure-proxy.example.invalid:8443";
    }
    return "PROXY proxy.example.invalid:8080";
}
