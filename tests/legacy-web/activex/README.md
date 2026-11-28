# ActiveX compatibility boundary

The classification module recognizes selected historical HTML object class IDs.
It is a library foundation, not a COM host, a plug-in installer, or a browser
DOM integration. Unknown and malformed identifiers have no replacement.
A recognized identifier indicates a candidate replacement only.

* Flash: existing Ruffle integration.
* Windows Media Player and QuickTime: candidate native media adapters.
* All other controls: preserve ordinary HTML fallback content.

No remote CODEBASE downloads, OCX/DLL loading, ActiveXObject bridge, COM
activation, or weakening of WebKit's sandbox is implemented. A future
replacement handler should be explicit, allowlisted, and use only the
minimum data required by that provider. Original controls that cannot be
replaced should be handled outside the browser in an isolated environment.

A control ID does not authenticate a website or prove compatibility,
installation, or permission to execute anything.
