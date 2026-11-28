envs = environment()
envs.set('G_TEST_SRCDIR', meson.current_source_dir())
envs.set('G_TEST_BUILDDIR', meson.current_build_dir())
envs.set('GSETTINGS_SCHEMA_DIR', join_paths(meson.project_build_root(), 'data'))
envs.set('GSETTINGS_BACKEND', 'memory')
envs.set('GTK_IM_MODULE', 'none')
envs.prepend('PATH', join_paths(meson.project_build_root(), 'src'))

test_cargs = ['-UG_DISABLE_ASSERT']

if get_option('unit_tests').allowed()
  if php_diagnostics.allowed()
    php_for_tests = find_program('php', native: true, required: false)
    if php_for_tests.found()
      test('PHP syntax diagnostics', find_program('sh', native: true),
        args: [files('php-lint-test.sh'), files('../src/developer/epiphany-php-lint'), php_for_tests.full_path()],
      )
    endif
  endif

  proxy_test = executable('test-ephy-proxy',
    'ephy-proxy-test.c',
    dependencies: [gio_dep, glib_dep, libsoup_dep],
    c_args: test_cargs,
  )
  test('Proxy resolver contract', proxy_test)

  cdf_test = executable('test-ephy-cdf',
    'ephy-cdf-test.c',
    '../lib/ephy-cdf.c',
    include_directories: include_directories('../lib'),
    dependencies: [gio_dep, glib_dep, libxml_dep],
    c_args: test_cargs,
  )
  test('CDF parser', cdf_test)

  gopher_test = executable('test-ephy-gopher',
    'ephy-gopher-test.c',
    '../lib/ephy-gopher.c',
    include_directories: include_directories('../lib'),
    dependencies: [gio_dep, glib_dep],
    c_args: test_cargs,
  )
  test('Gopher protocol parsing', gopher_test)

  tftp_test = executable('test-ephy-tftp',
    'ephy-tftp-test.c',
    '../lib/ephy-tftp.c',
    include_directories: include_directories('../lib'),
    dependencies: [gio_dep, glib_dep],
    c_args: test_cargs,
  )
  test('TFTP URI parsing', tftp_test)

  ruffle_test = executable('test-ephy-ruffle',
    'ephy-ruffle-test.c',
    '../lib/ephy-ruffle.c',
    include_directories: include_directories('../lib'),
    dependencies: [config_h, gio_dep, glib_dep],
    c_args: test_cargs,
  )
  test('Ruffle integration', ruffle_test)

  libephytestutils = static_library('ephytestutils',
    'ephy-test-utils.c',
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )

  ephytestutils_dep = declare_dependency(
    link_with: libephytestutils
  )

  # FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=778153
  # download_test = executable('test-ephy-download',
  #   'ephy-download-test.c',
  #   dependencies: ephymain_dep,
  #   c_args: test_cargs,
  # )
  # test('Download test',
  #      download_test,
  #      env: envs
  # )

  embed_shell_test = executable('test-ephy-embed-shell',
    'ephy-embed-shell-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs + ['-DTEST_DIR="' + meson.current_source_dir() + '"'],
  )
  test('Embed shell test',
       embed_shell_test,
       env: envs
  )

  embed_utils_test = executable('test-ephy-embed-utils',
    'ephy-embed-utils-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('Embed utils test',
       embed_utils_test,
       env: envs
  )

  encodings_test = executable('test-ephy-encodings',
    'ephy-encodings-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('Encodings test',
       encodings_test,
       env: envs
  )

  file_helpers_test = executable('test-ephy-file-helpers',
    'ephy-file-helpers-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('File helpers test',
       file_helpers_test,
       env: envs
  )

  history_test = executable('test-ephy-history',
    'ephy-history-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('History test',
       history_test,
       env: envs
  )

  location_entry_test = executable('test-location-entry',
    'ephy-location-entry-test.c',
    resources,
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('Location entry test',
       location_entry_test,
       env: envs
  )

  migration_test = executable('test-ephy-migration',
    'ephy-migration-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('Migration test',
    migration_test,
    env: envs,
    depends: ephy_profile_migrator
  )

  search_engine_manager_test = executable('test-ephy-search-engine-manager',
    'ephy-search-engine-manager-test.c',
    dependencies: ephymisc_dep,
    c_args: test_cargs,
  )
  test('Search engine manager test',
    search_engine_manager_test,
    env: envs,
    depends: compile_schemas,
  )

  # FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=707220
  # session_test = executable('test-ephy-session',
  #   'ephy-session-test.c',
  #   dependencies: ephytestutils_dep,
  #    c_args: test_cargs,
  # )
  # test('Session test',
  #      session_test,
  #      env: envs
  # )

  shell_test = executable('test-ephy-shell',
    'ephy-shell-test.c',
    resources,
    readability_resources,
    dependencies: [ephymain_dep, ephytestutils_dep],
    c_args: test_cargs,
  )
  test('Shell test',
       shell_test,
       env: envs
  )

  snapshot_service_test = executable('test-ephy-snapshot-service',
    'ephy-snapshot-service-test.c',
    dependencies: [ephymain_dep, ephytestutils_dep],
    c_args: test_cargs,
  )
  test('Snapshot service test',
       snapshot_service_test,
       env: envs
  )

  sqlite_test = executable('test-ephy-sqlite',
    'ephy-sqlite-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('SQLite test',
       sqlite_test,
       env: envs
  )

  output_encoding_test = executable('test-ephy-output-encoding',
    'ephy-output-encoding-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('Output encoding test',
       output_encoding_test,
       env: envs
  )

  string_test = executable('test-ephy-string',
    'ephy-string-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('String test',
       string_test,
       env: envs
  )

  uri_helpers_test = executable('test-ephy-uri-helpers',
    'ephy-uri-helpers-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('URI helpers test',
    uri_helpers_test,
    env: envs
  )

  web_app_utils_test = executable('test-ephy-web-app-utils',
    'ephy-web-app-utils-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('Web app utils test',
       web_app_utils_test,
       env: envs
  )

  web_view_test = executable('test-ephy-web-view',
    'ephy-web-view-test.c',
    resources,
    readability_resources,
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('Web view test',
       web_view_test,
       env: envs
  )

  web_extension_test = executable('test-ephy-web-extension',
    'ephy-web-extension-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs
  )
  test('Web extension test',
        web_extension_test,
        env: envs
  )

  sync_service_test = executable('test-ephy-sync-service',
    'ephy-sync-service-test.c',
    dependencies: [ephymain_dep, ephysync_dep],
    c_args: test_cargs,
  )
  test('Sync service test',
       sync_service_test,
       env: envs
  )
endif
