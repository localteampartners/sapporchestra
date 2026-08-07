# DEPENDENCIES — sapporchestra

<!-- UPDATE WHEN: an external service, API, account, or library is added/removed -->

| Dependency | Kind | Pin / source | Notes |
|---|---|---|---|
| SappSounds | sibling library | ../sappsounds (FetchContent fallback: github main) | the sample engine; Sapp::Sounds |
| JUCE | build dep | 8.0.15 (FetchContent) | plugin/UI framework |
| Catch2 | test dep | v3.7.1 (FetchContent) | unit tests |
| dr_flac | vendored (via SappSounds) | third_party, public domain | FLAC decode |
| Sonatina Symphonic Orchestra | content (user-downloaded) | github peastman/sso | FULL ORCHESTRA preset target; CC Sampling Plus |
| Virtual Playing Orchestra 3 | content (user-downloaded) | virtualplaying.com / archive.org | DXF crossfade patches |
| VSCO 2 CE | content (user-downloaded) | github sgossner/VSCO-2-CE | CC0 |
| GitHub Actions | CI | .github/workflows/release-builds.yml | attaches Windows+macOS zips to every release |
| SappLink manifest | contract | ~/apps/sapptune/sapplink/manifests/sapporchestra.json | vendored copy drift-guarded in tests/data |

No servers, no accounts, no secrets.
