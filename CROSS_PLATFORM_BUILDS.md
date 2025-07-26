# Cross-Platform Release Build Workflow

This repository includes an automated workflow that builds cross-platform packages whenever a new tag is created.

## Supported Platforms

The workflow automatically builds packages for the following architectures:

- **linux-x64**: Native x86_64 Linux build (includes examples and tools)
- **linux-arm64**: ARM64/AArch64 Linux build (cross-compiled, library only)
- **linux-arm32**: ARM32 Linux build (cross-compiled, library only)

## How to Trigger a Release

1. Create a new git tag:
   ```bash
   git tag v1.0.0
   git push origin v1.0.0
   ```

2. The workflow will automatically:
   - Build the library for all supported architectures
   - Package each build with headers and documentation
   - Create a GitHub release with all packages attached

## Package Contents

Each package contains:
- `lib/libulog.a` - Static library for the target architecture
- `include/ulog/` - Header files for development
- `README.txt` - Platform-specific usage instructions

For the x64 build only:
- `examples/` - Example programs
- `tools/` - Additional tools like logroller

## Usage

1. Download the appropriate package for your target platform from the releases page
2. Extract the archive
3. Include the headers in your project: `-I./include`
4. Link against the static library: `-L./lib -lulog`

Example compilation:
```bash
gcc -I./include your_program.c -L./lib -lulog -o your_program
```

## Cross-Compilation Notes

- ARM builds are cross-compiled on Ubuntu using standard cross-compilation toolchains
- ARM packages contain only the core library to avoid linker compatibility issues
- For ARM targets, you may need appropriate runtime libraries on the target system
- Examples and tools are only included in the x64 build to ensure compatibility

## Workflow Details

The workflow is defined in `.github/workflows/release.yml` and:
- Triggers on any tag push matching `v*`
- Uses Ubuntu runners with cross-compilation toolchains
- Builds in Release mode with optimizations
- Creates compressed archives for each platform
- Uploads all packages to a single GitHub release

## Troubleshooting

If you encounter issues with the packages:

1. **ARM libraries not working**: Ensure you have the correct runtime libraries installed on your target system
2. **Linking errors**: Make sure you're using the correct architecture package for your target
3. **Missing examples**: Examples are only available in the x64 package - use those for reference

For issues with the workflow itself, check the Actions tab in the GitHub repository.