# GNOME Web

GNOME Web (codename: Epiphany) is a GNOME web browser based on
[the WebKit rendering engine](https://webkit.org/). The codename means "a
usually sudden manifestation or perception of the essential nature or meaning of
something" ([Merriam-Webster](https://www.merriam-webster.com/dictionary/epiphany)).

Epiphany is opinionated.

## Download and Install

Epiphany is designed for Linux systems. The recommended way to install Epiphany
is via [Flatpak](https://www.flatpak.org/). You may:

 * [Download the latest stable version from Flathub](https://flathub.org/apps/details/org.gnome.Epiphany)
   (recommended).
 * [Download Epiphany Technology Preview](https://nightly.gnome.org/repo/appstream/org.gnome.Epiphany.Devel.flatpakref)
   if you are adventurous and want to help test tomorrow's Epiphany today. It
   is **not stable**.

 * Download Epiphany Canary if you are even more adventurous and want to help
   test the most recent development versions of WebKitGTK and Epiphany. This
   flavor of Epiphany is more likely to be **very unstable** because the code
   being built comes directly from WebKit's git main branch and Epiphany's git
   master branch. Epiphany Canary can be installed by executing the following
   flatpak command in a terminal:

   ```shell
   flatpak --user install https://nightly.gnome.org/repo/appstream/org.gnome.Epiphany.Canary.flatpakref
   ```

Epiphany is probably also available via your operating system's package manager,
but such packages are often outdated and insecure. Flatpak is the best
application distribution mechanism for Linux.

## Building from Source

### The Easy Way

The recommended way to build Epiphany locally is using the flatpak-builder
manifest with GNOME Builder. After installing Builder and launching it, you'll
see the Select a Project page. Select Open, then select the toplevel Epiphany
directory. Builder will detect the org.gnome.Epiphany.json flatpak-builder
manifest and you will be able to build the project in Builder. All required
dependencies will be provided by the manifest.

### Building Manually

Epiphany uses the [Meson build system](https://mesonbuild.com/) (1.3 or newer).
A compiler supporting GNU C23, or its GNU C2x draft spelling, is required.
Legacy Channel Definition Format (CDF) documents can be opened locally using
the browser's Open File command. The read-only channel viewer supports nested
channels, titles, abstracts and HTTP(S) links. Relative links require an
absolute HTTP(S) SELF URL. It does not automatically fetch channel content,
load external DTDs, run embedded scripts or schedule subscriptions.

Historical JScript script declarations are covered by the Web view regression
suite in tests/legacy-web/jscript. The test checks the shared ECMAScript
behavior and legacy text/jscript media type; it does not emulate Microsoft
conditional compilation or Windows COM objects.

Firefox Sync requires matching Nettle and Hogweed releases. Meson checks the
installed GMP configuration and links a public-key API probe before building.
Normal GMP builds are preferred; mini-GMP is not ABI-compatible with them.
You can build
Epiphany the same way you would any software that uses Meson. For example:

```
$ mkdir build && cd build
$ meson ..
$ ninja
$ sudo ninja install
```

You will have to install several pkg-config dependencies. If you are missing a
dependency, meson will present an error that looks like this:

```
meson.build:84:0: ERROR:  Native dependency 'hogweed' not found
```

In RPM-based distributions, you can install the missing dependencies
automatically. For example, in Fedora:

```
$ sudo dnf install 'pkgconfig(hogweed)'
```

In deb-based distributions:

```
$ sudo apt install $(apt-file search --package-only hogweed)
```

In other distributions, you must research each dependency to determine which
package provides the required pkg-config file.

### Homebrew discovery on macOS

For a native macOS build, an optional environment helper makes existing Homebrew
libraries and build tools visible to Meson, including selected unlinked
(keg-only) formulae. From the source directory, run:

```sh
sh build-aux/homebrew-env.sh meson setup build
sh build-aux/homebrew-env.sh meson compile -C build
sh build-aux/homebrew-env.sh meson test -C build
```

For an existing build directory, use `meson setup --reconfigure build` through
the helper. Use the same helper for build commands that may reconfigure the
project. The helper checks `brew` on `PATH`, then the standard Homebrew location
for the running architecture. Use a Homebrew installation matching your target
architecture. Existing `PATH` and `PKG_CONFIG_PATH` entries retain priority;
additional paths apply only to the invoked command. No packages are installed or
updated. Additional formulae can be exposed through those environment variables.
Use explicit Meson machine files instead for cross builds.

Native macOS builds use AppKit and Foundation rather than libportal-gtk4.
Opening URLs and revealing files use NSWorkspace. Native web app installation,
launcher metadata, removal and background permission requests are not yet
implemented on macOS. They are disabled rather than reported as successful.
The Linux XDG portal backend is unchanged.
On macOS, generated website passwords use Security.framework's secure random
number generator instead of libpwquality. Other platforms retain libpwquality.
The memory diagnostics page uses Mach task information on macOS and reports
only the browser process; Linux retains its /proc/smaps breakdown. The
freedesktop default-browser prompt is disabled on macOS until native app
bundle registration and default-browser integration are implemented.

This is dependency discovery support, not a complete macOS port. A compatible
WebKitGTK build and all other required libraries and tools must still be
available, and the normal Meson version checks remain in effect.

### Rebuilding Dependencies

Newcomers should try to avoid bugs that require making changes in WebKit or
other dependencies of Epiphany. Once you've contributed a few easier patches to
Epiphany and are ready to start making code changes in WebKit or other
dependencies, then your development setup becomes a bit more complicated. You
are welcome to use whatever method you prefer for development, but the
recommended method is to use either [Toolbox](https://containertoolbx.org/) or
[JHBuild](https://gnome.pages.gitlab.gnome.org/jhbuild/index.html). Here is an
example of how to build your own WebKit using JHBuild, assuming your JHBuild
installation prefix is `$HOME/jhbuild install/`:

```
$ git clone https://github.com/WebKit/WebKit.git WebKit
$ mkdir -p WebKit/WebKitBuild/GNOME
$ cd WebKit/WebKitBuild/GNOME
$ jhbuild run cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DPORT=GTK -DDEVELOPER_MODE=ON -DCMAKE_INSTALL_PREFIX=$HOME/jhbuild/install/ -DCMAKE_INSTALL_LIBDIR=lib -GNinja ../..
$ jhbuild run ninja
$ jhbuild run cmake -P cmake_install.cmake
```

## Manifesto

A web browser is more than an application: it is a way of thinking, a way of
seeing the world. Epiphany's principles are simplicity, standards compliance,
and software freedom.

### Simplicity

Feature bloat and user interface clutter is evil.

Epiphany aims to present the simplest interface possible for a browser. Simple
does not necessarily mean less-powerful. The commonly-used browsers of today are
too big, buggy, and bloated. Epiphany is a small browser designed for the web:
not for mail, newsgroups, file management, instant messaging, or coffeemaking.
The UNIX philosophy is to design small tools that do one thing and do it well.

### Standards Compliance

The introduction of nonstandard features in browsers could make it difficult
or impossible to use alternative products like Epiphany if developers embrace
them. Alternative standards-complying browsers might not be able to fully access
websites making use of these features. The success of nonstandard features can
ultimately lead one browser to dominate the market.

Standards compliance ensures the freedom of choice. Epiphany aims to achieve
this.

### Software Freedom

Epiphany is not just free of cost; more importantly, the source code is made
available to you under a license that [respects your freedom](https://www.gnu.org/philosophy/philosophy.html).

Just as GNOME exists to oppose proprietary desktop software, Epiphany opposes
the dominance of the web by proprietary software web browsers. Today's chief
offender is Google Chrome, a browser that purports to be open source, yet
actually includes several proprietary components. In contrast, Epiphany is fully
free software.

## Human Interface

Epiphany follows the [GNOME Human Interface Guidelines](https://developer.gnome.org/hig).
Unless there are serious reasons to make an exception, not following the
guidelines will be considered a bug.

### GNOME Integration

Epiphany's main goal is to be integrated with GNOME, as well as similar
desktops (notably elementary OS). We don't aim to make Epiphany usable outside
these environments.

### Preferences

We are cautious about adding new preferences. Preferences can be added when they
make sense, but they should always be carefully-considered.
[Preferences come with a cost](https://ometer.com/preferences.html).

### Target Audience

We target nontechnical users by design. This happens to be 90% of the user
population. Technical details should not be exposed in the interface.

We target web users, not web developers. A few geek-oriented features, like the
web inspector, are welcome so long as they are non-obtrusive.

## Privacy Policy

The GNOME Web developers are grateful that Mozilla allows third-party use of its
Firefox Sync service. The following privacy policy statement is required to be
displayed on this website in order to comply with the terms of service for
Firefox Sync:

### Firefox Sync

By default, GNOME Web collects personal data, such as your bookmarks and web
browsing history. This data is stored locally on your computer for your own
personal use. It is never transmitted to the GNOME Web developers. It is never
transmitted to any third party unless you choose to enable optional Sync
integration. If Sync integration is enabled, your data is encrypted and
transmitted to Mozilla's Firefox Sync server, where it is stored in encrypted
form. Although GNOME Web uses the Firefox Sync service, GNOME Web is not
Firefox and is not produced or endorsed by Mozilla.

### Tracking Prevention

GNOME Web enables Intelligent Tracking Prevention by default. It includes many
other tracking prevention measures. [Refer to WebKit's tracking prevention
documentation for full details.](https://webkit.org/tracking-prevention/)

### Adblock

GNOME Web regularly downloads updated adblock filter lists from the
ublock-webkit-filters GitHub project. Users who are concerned about GNOME Web
automatically connecting to GitHub may disable this feature in Preferences.

## Contact Us

The recommended way to contact us is via the
[Epiphany discussion forum](https://discourse.gnome.org/tag/epiphany).
