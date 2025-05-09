# Tests for BitScrape

# Add unit tests
subdir('unit')

# Add regression tests
# These are shell scripts, so we don't need to build them
# But we can add them as tests to Meson
run_regression_tests = find_program('run_regression_tests.sh')
test('Regression Tests', run_regression_tests, workdir: meson.current_source_dir())
