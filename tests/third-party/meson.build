# Third-party array types through the data hooks: its own executable, so a dependency the machine
# does not have never blocks the main suite, and its own test, so the whole-suite pass covers it.
eigen_dep = dependency('eigen3', required: false, include_type: 'system',
                       fallback: ['eigen', 'eigen_dep'])
xtensor_dep = dependency('xtensor', required: false, include_type: 'system',
                         fallback: ['xtensor', 'xtensor_dep'])
mdspan_dep = dependency('mdspan', required: false, include_type: 'system',
                        fallback: ['mdspan', 'mdspan_dep'])

if eigen_dep.found() and xtensor_dep.found() and mdspan_dep.found()
    third_party_moc_files = qtmod.compile_moc(
        headers: ['test-third-party-arrays/test-third-party-arrays.h'])
    third_party_arrays = executable(
        'third-party-arrays',
        ['test-third-party-arrays/test-third-party-arrays.cpp', third_party_moc_files],
        dependencies: [NeoQCP_dep, qtdeps, qttest, eigen_dep, xtensor_dep, mdspan_dep],
        override_options: ['unity=off'])
    test('third-party-arrays', third_party_arrays, timeout: 300, env: tests_env)
endif
