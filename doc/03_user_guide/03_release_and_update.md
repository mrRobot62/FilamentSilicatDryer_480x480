# Release and Update Workflow

This page defines the currently documented release and update baseline for the project.

The goal is not yet a fully automated release process. The goal is to make firmware state, migration expectations and update handling reproducible for future maintainers and rebuilds.

## Version sources

The current firmware version is defined in:

- [include/versions.h](/Users/bernhardklein/workspace/arduino/esp32/FilamentSilicatDryer_480x480/include/versions.h)
- [CHANGELOG.md](/Users/bernhardklein/workspace/arduino/esp32/FilamentSilicatDryer_480x480/CHANGELOG.md)

At runtime, the visible release identity is composed from:

- semantic version, for example `0.7.3`
- build id derived from git history
- build date embedded in the firmware

## Release baseline

For the current documentation state, a release should always answer these questions clearly:

- which semantic version is active
- which functional focus changed
- whether `HOST` and `CLIENT` must be updated together
- whether configuration or persisted parameters need migration handling
- whether new safety-relevant behavior was introduced

## Recommended release checklist

Before publishing or tagging a release:

1. update [include/versions.h](/Users/bernhardklein/workspace/arduino/esp32/FilamentSilicatDryer_480x480/include/versions.h)
2. update [CHANGELOG.md](/Users/bernhardklein/workspace/arduino/esp32/FilamentSilicatDryer_480x480/CHANGELOG.md)
3. build `HOST` and `CLIENT`
4. verify basic communication and boot behavior
5. verify parameter persistence if related code changed
6. review active documentation links from [README.md](/Users/bernhardklein/workspace/arduino/esp32/FilamentSilicatDryer_480x480/README.md)
7. publish firmware artifacts and flashing notes if needed

## Update baseline

Until a stricter release process exists, treat updates conservatively:

- prefer updating `HOST` and `CLIENT` as a matching pair
- do not assume forward/backward compatibility across arbitrary mixed firmware versions
- if parameter storage changed, document reset or migration expectations explicitly
- after flashing, verify displayed version and basic runtime behavior before normal use

## Post-update verification

After an update, verify at minimum:

- correct version is shown on the device
- UI boots normally
- `HOST` and `CLIENT` link up correctly
- temperatures are plausible
- actuator icons match real hardware state
- parameter storage still behaves as expected

## Current limitations

The project does not yet provide:

- a formal migration matrix between all historical versions
- an automated release pipeline
- a guaranteed no-reset upgrade path for all future NVM changes
- a published compatibility table for hardware variants

Those items are planned follow-up work on the path from a strong engineering prototype toward a more reproducible reference platform.
