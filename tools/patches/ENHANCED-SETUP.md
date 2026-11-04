# Enhanced tool changes

The XenonRecomp-enhanced.patch file captures the complete local tool changes against the pinned XenonRecomp commit ddd128bcca99fe8bfbb99bea583c972351fa6ace.

Apply it to a clean submodule checkout instead of the other XenonRecomp patches (it includes the local combined changes):

```
git submodule update --init tools/XenonRecomp
git -C tools/XenonRecomp apply ../patches/XenonRecomp-enhanced.patch
```

Generated recompilation output and original game files are excluded. The hitmarker image and sound were supplied separately, as confirmed by the project owner.
