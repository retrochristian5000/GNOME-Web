highlightjs_resource_files = files('highlightjs/highlightjs.gresource.xml')
highlightjs_resources = gnome.compile_resources('highlightjs-resources',
    highlightjs_resource_files,
    c_name: 'highlightjs',
    source_dir: 'highlightjs'
)

readability_resource_files = files('readability/readability.gresource.xml')
readability_resources = gnome.compile_resources('readability-resources',
    readability_resource_files,
    c_name: 'readability',
    source_dir: 'readability'
)

libgvdb_sources = [
  'gvdb/gvdb-builder.c',
  'gvdb/gvdb-reader.c'
]

libgvdb_deps = [
  gio_dep,
  glib_dep
]

libgvdb_includes = [
  'gvdb/'
]

libgvdb = static_library('gvdb',
  libgvdb_sources,
  dependencies: libgvdb_deps,
  include_directories: libgvdb_includes
)

gvdb_dep = declare_dependency(
  link_with: libgvdb,
  include_directories: libgvdb_includes,
  dependencies: libgvdb_deps
)

# Package only the public JavaScript/Wasm assets from a supplied release.
# No automatic network downloads, npm installs or build-host runtime paths.
if have_ruffle_web
  python = import('python').find_installation('python3')
  ruffle_manifest_script = files('../build-aux/ruffle-web-assets.py')
  ruffle_asset_names = run_command(python, ruffle_manifest_script,
    '--list', ruffle_web_dir, check: true).stdout().strip().split('\n')
  ruffle_asset_inputs = []
  foreach name : ruffle_asset_names
    ruffle_asset_inputs += [ruffle_web_dir / name]
  endforeach
  ruffle_manifest = configure_file(
    output: 'ruffle-web.gresource.xml',
    command: [python, ruffle_manifest_script, '--manifest', ruffle_web_dir],
    capture: true,
  )
  ruffle_web_resource = custom_target('ruffle-web-resource',
    input: [ruffle_manifest] + ruffle_asset_inputs,
    output: 'ruffle-web.gresource',
    command: [find_program('glib-compile-resources', native: true),
      '--sourcedir', ruffle_web_dir, '--target', '@OUTPUT@', '@INPUT0@'],
    build_by_default: true,
    install: true,
    install_dir: pkgdatadir,
  )
  if get_option('unit_tests').allowed()
    test('Ruffle Web asset manifest', python,
      args: [files('../tests/ruffle-web-assets-test.py'), ruffle_manifest_script])
  endif
  # Retain upstream licensing and notices with the installed distribution.
  foreach notice : ['LICENSE', 'LICENSE.md', 'LICENSE-MIT', 'LICENSE-APACHE', 'README.md']
    if fs.is_file(ruffle_web_dir / notice)
      install_data(ruffle_web_dir / notice, install_dir: pkgdatadir / 'ruffle-web-notices')
    endif
  endforeach
endif
