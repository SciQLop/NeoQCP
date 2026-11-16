# Under ASan: catch use of a stack event after its handler returned, and fill freed memory so that
# Qt code (not instrumented) reading a deleted widget crashes instead of reading stale data.
tests_env = environment()
if 'address' in get_option('b_sanitize')
    tests_env.append('ASAN_OPTIONS',
                     'halt_on_error=1:abort_on_error=1:print_summary=1:'
                     + 'detect_stack_use_after_return=1:max_free_fill_size=1048576:free_fill_byte=190',
                     separator: ':')
endif

subdir('auto')
subdir('third-party')
subdir('benchmark')
subdir('manual')
subdir('big_data_for_tracy')
subdir('perf')

# Guards the signal rules of M4a: direct emits outside the allow-list, overloaded signal names.
test('check-signals',
     python3,
     args: [meson.project_source_root() / 'tools' / 'check-signals.py',
            meson.project_source_root() / 'src'])
