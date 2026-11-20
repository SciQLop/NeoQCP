# The milestone-6 target programs: built so a change that breaks one fails the build. All eleven
# compile, annotations since 6h (see README.md).
foreach target : ['shared-x', 'symlog', 'spectrogram', 'lollipop', 'callout', 'peak-fit',
                  'brick-breaker', 'asteroids', 'fill-between', 'inset-zoom', 'annotations']
  executable('user-code-' + target,
    target / 'main.cpp',
    dependencies: [NeoQCP_dep]
  )
endforeach
