subdir('auto')
subdir('benchmark')
subdir('manual')
subdir('big_data_for_tracy')
subdir('perf')

# Guards the signal rules of M4a: direct emits outside the allow-list, overloaded signal names.
test('check-signals',
     python3,
     args: [meson.project_source_root() / 'tools' / 'check-signals.py',
            meson.project_source_root() / 'src'])
