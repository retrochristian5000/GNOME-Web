# JScript compatibility baseline

Original test fixture, based on HTML 4-era script syntax and modern HTML's
historical script-type handling. Serve this directory over HTTP with
`external.js` and `external-type.js` using the Content-Type `text/jscript`.
The existing EphyWebView test server provides those exact response types.

Expected: both inline and both external script declarations execute;
the common ECMAScript operations succeed; unknown script types do not
execute; Microsoft conditional-compilation comments remain comments.

References:
- https://html.spec.whatwg.org/multipage/scripting.html
- https://www.rfc-editor.org/rfc/rfc9239.html
- https://learn.microsoft.com/en-us/openspecs/ie_standards/ms-es3/

This is a baseline, not a JScript 5.x conformance claim. Microsoft-only
syntax, Enumerator, VBArray and Windows COM/ActiveX are not implemented
by this fixture. Any future compatibility backend must preserve normal
script-type checking, CSP and origin boundaries.
