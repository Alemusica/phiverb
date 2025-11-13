# Documentation Index

This directory contains comprehensive documentation for Wayverb development.

## Quick Start

- **New Developers:** Start with `INFRASTRUCTURE.md`
- **PR Authors:** Use `CONTROL_ROOM.md` for workflow commands
- **Metal Development:** See `APPLE_SILICON.md`
- **Work Tracking:** Check `action_plan.md`

## Infrastructure Documentation

### Core Guides

| Document | Purpose | Audience |
|----------|---------|----------|
| `INFRASTRUCTURE.md` | Complete infrastructure overview | All developers |
| `CONTROL_ROOM.md` | Command reference and workflows | PR authors |
| `action_plan.md` | Current work tracking | All team |
| `archeology.md` | TODO and comment tracking | Maintainers |

### Specialized Guides

| Document | Purpose | Audience |
|----------|---------|----------|
| `APPLE_SILICON.md` | Metal backend and M-series optimization | Metal developers |
| `metal_port_plan.md` | Metal porting strategy | Metal developers |
| `AGENT_PROMPTS.md` | Agent configuration | Agent users |
| `METAL_FIX_SUMMARY.md` | Metal debugging history | Metal developers |

### Reference Documentation

| Document | Purpose | Audience |
|----------|---------|----------|
| `USAGE_EXAMPLES.md` | Example commands and scripts | All developers |
| `mesh_tools.md` | Geometry processing tools | 3D modelers |
| `development_log.md` | Historical changes | Maintainers |
| `IMPLEMENTATION_SUMMARY.md` | Implementation details | Core developers |

## External Documentation

- **Repository Root:**
  - `../readme.md` - Main project README
  - `../AGENTS.md` - Agent operating instructions
  - `../.codex/POLICY_ASK.md` - ASK system policy

- **GitHub:**
  - `../.github/workflows/README.md` - Workflow summary
  - `../.github/agents/README.md` - Custom agents
  - `../.github/pull_request_template.md` - PR template

- **Generated:**
  - `../analysis/comment_summary.md` - Comment inventory
  - `../analysis/comment_inventory.json` - Raw comment data
  - `../analysis/todo_comment_xref.md` - TODO cross-reference

## Documentation Structure

```
phiverb/
├── readme.md                    # Main project README
├── AGENTS.md                    # Agent instructions
├── .codex/
│   └── POLICY_ASK.md           # ASK system policy
├── .github/
│   ├── pull_request_template.md
│   ├── workflows/
│   │   └── README.md           # Workflow summary
│   └── agents/
│       └── README.md           # Custom agents
├── docs/                       # This directory
│   ├── README.md              # This file
│   ├── INFRASTRUCTURE.md      # Infrastructure overview ⭐
│   ├── CONTROL_ROOM.md        # Command reference ⭐
│   ├── action_plan.md         # Work tracking ⭐
│   ├── archeology.md          # TODO tracking
│   ├── APPLE_SILICON.md       # Metal/M-series guide
│   ├── metal_port_plan.md     # Metal strategy
│   ├── AGENT_PROMPTS.md       # Agent config
│   ├── USAGE_EXAMPLES.md      # Examples
│   └── [other docs]
└── analysis/
    ├── comment_summary.md      # Auto-generated
    └── comment_inventory.json  # Auto-generated
```

## Documentation Workflow

### For Developers

1. **Before starting work:**
   - Read `INFRASTRUCTURE.md` for overview
   - Check `action_plan.md` for current priorities
   - Review `CONTROL_ROOM.md` for commands

2. **During development:**
   - Use `/ask` for design questions
   - Update `action_plan.md` checkboxes
   - Follow PR template checklist

3. **Before merging:**
   - Update relevant docs if behavior changed
   - Reference `AP-XXX-NNN` in PR description
   - Ensure ASK requests resolved

### For Maintainers

1. **Daily:**
   - Review ASK requests (`.codex/NEED-INFO/`)
   - Monitor CI health
   - Triage new issues

2. **Weekly:**
   - Review `action_plan.md` progress
   - Check `archeology.md` for drift
   - Update priorities

3. **After release:**
   - Archive completed items
   - Update roadmap in `INFRASTRUCTURE.md`
   - Document lessons learned

## Auto-Generated Documentation

These files are automatically updated by `archeology-xref.yml`:

- `archeology.md` - Updated nightly and on PR
- `action_plan.md` - Refreshed with TODO scan
- `analysis/comment_summary.md` - Comment inventory
- `analysis/comment_inventory.json` - Raw data

**Do not edit manually** - use `tools/docs_refresh.py` instead.

## Finding Information

### "How do I...?"

| Question | Document | Section |
|----------|----------|---------|
| Submit a PR? | `CONTROL_ROOM.md` | Workflow Examples → Standard PR Flow |
| Create ASK request? | `CONTROL_ROOM.md` | ASK System |
| Run tests? | `CONTROL_ROOM.md` | Slash Commands |
| Build with Metal? | `APPLE_SILICON.md` | Building |
| Check my scope? | `CONTROL_ROOM.md` | /scope command |
| Update action plan? | `action_plan.md` | Header instructions |

### "What is...?"

| Term | Document | Definition |
|------|----------|------------|
| ASK System | `INFRASTRUCTURE.md` | PR #1 → Token Guard |
| Control Room | `CONTROL_ROOM.md` | Overview |
| Action Plan | `action_plan.md` | Header |
| Doc Gate | `INFRASTRUCTURE.md` | PR #1 → Doc Advisory |
| Split CI | `INFRASTRUCTURE.md` | PR #1 → Split CI Pipeline |
| Archeology XRef | `INFRASTRUCTURE.md` | PR #5 → Agent Iteration Router |

### "Where is...?"

| Item | Location |
|------|----------|
| Workflow files | `.github/workflows/` |
| ASK requests | `.codex/NEED-INFO/` |
| Script utilities | `scripts/`, `tools/` |
| Test scenes | `tests/scenes/` |
| Generated docs | `analysis/` |
| Custom agents | `.github/agents/` |

## Documentation Standards

### Writing Style

- Use clear, concise language
- Include examples for complex concepts
- Link to related documentation
- Keep sections focused and scannable

### Format Conventions

```markdown
# Title (H1) - Document title only

## Section (H2) - Main sections

### Subsection (H3) - Subsections

**Bold** - Important terms, commands
`code` - Inline code, file names
```bash
# Code blocks with language
```

[Links](file.md) - Relative links within docs
```

### Code Examples

Always include:
- Context (what does this do?)
- Full command (copy-pasteable)
- Expected output (when relevant)
- Error handling (when relevant)

### Tables

Use for:
- Reference information
- Quick comparisons
- Status tracking
- Command lists

## Contributing to Documentation

### Adding New Documentation

1. Create file in `docs/` directory
2. Follow naming convention: `UPPERCASE.md` or `lowercase.md`
3. Add entry to this `README.md`
4. Link from relevant other docs
5. Include in PR description

### Updating Existing Documentation

1. Maintain existing structure
2. Add changelog entry if major change
3. Update cross-references
4. Verify links still work
5. Test examples still work

### Reviewing Documentation PRs

Check for:
- [ ] Accurate information
- [ ] Clear writing
- [ ] Working examples
- [ ] Proper cross-links
- [ ] Updated README.md index

## Maintenance Schedule

### Automated (No Action Required)

- **Nightly:** Archeology XRef updates docs
- **On PR:** Comment summary posted
- **On Push:** Auto-commit to main

### Manual

- **Weekly:** Review for accuracy
- **Monthly:** Check for outdated information
- **Quarterly:** Major reorganization if needed

## Questions?

1. Check this index first
2. Search relevant documents
3. Create ASK request if design question
4. Open GitHub issue if documentation bug

## Version History

### 2025-11-13: Initial Index
- Created comprehensive documentation index
- Added structure overview
- Documented workflow and standards
- Linked all existing documentation

---

**Last Updated:** 2025-11-13 by Infrastructure Consolidation PR
