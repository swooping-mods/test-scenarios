# Fast Test Scenarios: behavior and implementation

Describes the exported implementation **0.1.0**. The linked source and metadata define this version; gameplay qualification is a separate requirement.

## Behavior

Registers production-integration cases with Test Framework. Provider cases exercise actual Shop offers, authored prices, insufficient-funds/duplicate-purchase refusal, exact native debit, ownership and Equipment selection. Progression cases inspect/unlock genuine native prerequisites for Drone and Backpack; Zoom checks actual camera field-of-view behavior. Actions call real registered providers, not parallel simulated purchase implementations.

## State and lifecycle

Requires explicit Test Framework activation and its isolation capability plus Shop/Equipment services. Cases keep rooted world/player/provider context and revalidate before mutation/cleanup. Native fixture wallet/progression/loadout changes belong solely to the admitted disposable run; the scenario module has no independent player save format.

## Integration and configuration

Runtime dependencies: [test-framework](https://github.com/swooping-mods/test-framework), [shop-framework](https://github.com/swooping-mods/shop-framework), [equipment-framework](https://github.com/swooping-mods/equipment-framework).

The [module metadata](../source/mods/test-scenarios/mod.json) owns the version and declared integration contract. Native modules start at `MoonLab_ModStart`. Game/Unity integrations target the pinned Steam Windows x64 IL2CPP build.

| Service | ABI | Table size |
|---|---:|---|
| Requires `test-framework.testing` | 1 | at least 24 bytes |
| Requires `test-framework.isolation` | 1 | at least 24 bytes |
| Requires `shop-framework.shop` | 1 | at least 56 bytes |
| Requires `equipment-framework.equipment` | 1 | at least 32 bytes |

## Implementation sources

- [mod.c](../source/mods/test-scenarios/mod.c)
- [progression.h](../source/mods/test-scenarios/progression.h)
- [zoom.h](../source/mods/test-scenarios/zoom.h)

## Verification boundary

A passed purchase case does not qualify a provider’s flight, cleaning, sticker placement, cargo, save reload, rendered UI or real co-op. Missing provider or isolated fixture prerequisites must remain explicit unavailable cases.

`python3 build.py --check` verifies the exported source hashes. It does not execute the mod or qualify the behaviors above. Installation and independent build prerequisites are in the [README](../README.md).
