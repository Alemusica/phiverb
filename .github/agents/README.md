# Phiverb Custom Agents

This directory contains custom GitHub Copilot agents specialized for Phiverb development.

## Available Agents

### Phiverb Metal Assistant (`phiverb-metal-assistant.agent.md`)

Specialized assistant for Metal API porting, Apple Silicon optimization, and audio rendering debugging.

**Capabilities:**
- Metal API expertise (OpenCL to Metal conversion)
- Audio DSP support (FDTD, binaural processing, impulse response)
- Code analysis and debugging
- Performance optimization

**Usage with GitHub CLI:**
```bash
# Install GitHub CLI and Copilot extension if not already installed
# brew install gh
# gh extension install github/gh-copilot

# Use the agent
gh copilot chat -a .github/agents/phiverb-metal-assistant.agent.md "How do I fix clBuildProgram error?"

# Example queries:
gh copilot chat -a .github/agents/phiverb-metal-assistant.agent.md "Optimize my FDTD code for M2 Max"
gh copilot chat -a .github/agents/phiverb-metal-assistant.agent.md "Convert this OpenCL kernel to Metal"
gh copilot chat -a .github/agents/phiverb-metal-assistant.agent.md "Debug rendering artifacts in my mesh"
```

## Support Scripts

The agent works with these helper scripts in the `scripts/` directory:

- `scripts/test_metal.sh` - Test Metal shader compilation and run Metal-specific unit tests
- `scripts/validate_audio.py` - Validate audio output quality and format  
- `scripts/benchmark.sh` - Performance benchmarking for different rendering methods

**Running the scripts:**
```bash
# Test Metal implementation
./scripts/test_metal.sh

# Validate audio output
./scripts/validate_audio.py

# Run performance benchmarks
./scripts/benchmark.sh
```

## CI Integration

Metal-specific CI workflow is configured in `.github/workflows/metal-ci.yml`. It automatically:
- Builds the project with Metal enabled
- Tests Metal shader compilation
- Validates audio output
- Runs performance benchmarks

## Contributing

To add a new agent or improve existing ones:

1. Create or edit the `.agent.md` file in this directory
2. Follow the YAML frontmatter format with name, description, and version
3. Document capabilities, examples, and usage patterns
4. Update this README with the new agent information
5. Test the agent with GitHub CLI before committing

For more information, see the [GitHub Copilot Agents documentation](https://docs.github.com/en/copilot).
