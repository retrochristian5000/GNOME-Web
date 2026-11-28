# PAC resolver compatibility fixture

Use only in a disposable test environment with an explicitly configured
PAC URL. Serve proxy.pac from a trusted local HTTP server with MIME type
application/x-ns-proxy-autoconfig. Restore the prior system configuration
after the test.

Run build/src/epiphany-proxy-check against each URL below. The diagnostic
performs lookup only; do not connect to these placeholder proxies.

| URL | Expected ordered route |
| --- | --- |
| http://proxied.example.invalid/ | http://proxy.example.invalid:8080 |
| https://proxied.example.invalid/ | http://secure-proxy.example.invalid:8443 |
| https://direct.example.invalid/ | DIRECT |
| http://fallback.example.invalid/ | http://first.example.invalid:8080, http://second.example.invalid:8081, DIRECT |
| http://socks.example.invalid/ | socks5://socks-proxy.example.invalid:1080 |

Backend protocol and port spelling may differ. A fallback list must preserve
its intended order. The C test exercises manual and bypass rules and the
libsoup default resolver contract. It does not execute this PAC. Actual PAC
evaluation and WebKit network-process routing require native integration tests.
A DIRECT result alone does not establish success.
