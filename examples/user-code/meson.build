# The milestone-6 target programs that compile so far: built so a change that breaks one fails the
# build. The others wait for 6g and 6h (see README.md).
foreach target : ['shared-x', 'symlog', 'spectrogram', 'lollipop', 'callout', 'peak-fit',
                  'brick-breaker', 'asteroids']
  executable('user-code-' + target,
    target / 'main.cpp',
    dependencies: [NeoQCP_dep]
  )
endforeach
