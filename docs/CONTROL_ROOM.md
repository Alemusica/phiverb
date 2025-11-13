# Control Room — Wayverb Infrastructure Guide

This document describes the consolidated infrastructure and automation systems for Wayverb development.

## Overview

The Control Room is a collection of tools and workflows that provide:
- Continuous Integration with split builds (RT/DWM/Metal)
- Documentation consistency checks
- ASK system for blocking on critical questions
- Progress tracking and cross-referencing

## Components

### 1. CI/CD Pipeline (`.github/workflows/`)

#### Main CI (`ci-build-test.yml`)
Split CI pipeline that detects code changes and runs targeted tests:

**Jobs:**
- `detect`: Determines which components changed (raytracer/waveguide/metal)
- `rt-tests`: Runs raytracer unit tests and validation
- `dwm-tests`: Runs waveguide (DWM) tests with CFL/passivity checks
- `metal-smoke`: Tests Metal backend on self-hosted ARM64 runner

**Usage:**
- Automatically runs on all PRs
- Uses smart change detection (merge-base comparison)
- Skips unaffected components to save CI time

**Environment variables for Metal:**
```bash
WAYVERB_METAL=1              # Enable Metal backend
WAYVERB_METAL=force-opencl   # Force OpenCL fallback
WAYVERB_DISABLE_VIZ=1        # Disable GPU→CPU readbacks
WAYVERB_VIZ_DECIMATE=N       # Update pressures every N steps
WAYVERB_VOXEL_PAD=<int>      # Adjust voxel padding (default 5)
```

#### Advisory Workflows (Non-Blocking)

**Doc Gate (`doc-gate.yml`):**
- Checks if documentation is updated when code changes
- Looks for Action Plan references (`AP-XXX-NNN`)
- Advisory by default, blocking with `require-doc` label

**Ask Gate (`ask-gate.yml`):**
- **BLOCKING**: Fails if there are unresolved ASK requests
- Checks `.codex/NEED-INFO/` for open questions
- Forces resolution before merge

**Archeology XRef (`archeology-xref.yml`):**
- Runs nightly and on PR events
- Auto-generates documentation cross-references
- Updates `docs/archeology.md`, `docs/action_plan.md`, `analysis/comment_summary.md`

### 2. ASK System (Ask-Then-Act)

Located in `scripts/ask/` and `.codex/POLICY_ASK.md`

**Purpose:** Block merges when critical design decisions or data are needed.

**Creating a request:**
```bash
./scripts/ask/new \
  --title "ASK: What's the correct mass for PCS sphere?" \
  --question "Need formula for transparent source mass in waveguide" \
  --topic "waveguide"
```

This creates:
- File in `.codex/NEED-INFO/<timestamp>--<topic>.md`
- Git commit with the request
- GitHub issue (if `gh` CLI is available)
- **Blocks PR merge** until resolved

**Structure of ASK file:**
```markdown
# ASK — <Title>
**Topic:** <topic>
**Branch:** <branch> @ <sha>

## Background (riassunto)
<!-- 5-10 sentences describing context -->

## Exact question(s)
<numbered list of specific questions>

## Constraints / Acceptance
<!-- Numerical limits, formulas, timing, PASS criteria -->

## Technical context
- Policy reference
- Git diff
- Test logs
- QA logs

## Resolution (to fill by reviewer/GPT-5 Pro)
<!-- Paste final answer and references here -->
```

**Resolving a request:**
1. Update the `## Resolution` section with the answer
2. Apply the solution in code
3. Either:
   - Remove the file: `git rm .codex/NEED-INFO/<file>.md`
   - OR mark as `RESOLVED` in the Resolution section
4. Commit and push

**Gate behavior:**
- `ask-gate.yml` runs on every PR
- Checks for files in `.codex/NEED-INFO/`
- Fails if any file lacks `RESOLVED` in Resolution section
- Unblocks when all requests are resolved

### 3. Action Plan Tracking

**Location:** `docs/action_plan.md`

**Structure:**
- Organized by cluster (Raytracer, Waveguide, Metal, Materials, QA)
- Each item has:
  - Description and file paths
  - Checklist with acceptance criteria
  - Verification steps
  - Status tracking with IDs (`AP-<CLUSTER>-NNN`)

**Status values:**
- `OPEN`: Not started
- `IN-PROGRESS`: Currently being worked on
- `BLOCKED`: Waiting on dependency or ASK request
- `DONE`: Completed and verified

**Usage:**
- Reference in PR descriptions: `Relates: AP-RT-001`
- Update checkboxes as work progresses
- Add dated comments for significant milestones
- Auto-synced via `archeology-xref.yml`

### 4. PR Template

**Location:** `.github/pull_request_template.md`

**Format:**
```markdown
### Scopo
- [ ] RT (Raytracer)
- [ ] DWM (Digital Waveguide Mesh)
- [ ] Shared

### Checklist
- [ ] Build & test verdi
- [ ] `scripts/check_scope.py` OK
- [ ] QA suite pass (EDC/T20/T30, bounds)
- [ ] Niente refactor fuori scope
```

**Purpose:**
- Quick scope identification
- Standard verification steps
- Prevents scope creep
- Integrates with doc-gate checks

### 5. Slash Commands (Manual Patterns)

These are documented patterns for common operations:

#### `/ask` - Create ASK Request
```bash
./scripts/ask/new --title "ASK: <question>" --question "<details>" --topic "<cluster>"
```

#### `/scope` - Check PR Scope
```bash
python3 scripts/check_scope.py
```

#### `/monitor` - Watch App Logs
```bash
scripts/monitor_app_log.sh
```

#### `/qa` - Run Validation Suite
```bash
python3 scripts/qa/run_validation_suite.py --cli build/bin/wayverb_cli --scenes tests/scenes
```

#### `/regression` - Run Regression Tests
```bash
tools/run_regression_suite.sh
# With strict checking:
WAYVERB_ALLOW_SILENT_FALLBACK=0 tools/run_regression_suite.sh
```

#### `/xref` - Update Documentation Cross-References
```bash
python3 tools/docs_refresh.py
```

#### `/inventory` - Check Comment Changes
```bash
python3 tools/comment_inventory_diff.py
```

## Workflow Examples

### Standard PR Flow

1. **Create feature branch:**
   ```bash
   git checkout -b feature/my-change
   ```

2. **Make changes and check scope:**
   ```bash
   python3 scripts/check_scope.py
   ```

3. **If you need external info, create ASK:**
   ```bash
   ./scripts/ask/new --title "ASK: Need BRDF formula" --question "..." --topic "rt"
   ```

4. **Run local validation:**
   ```bash
   cmake -S . -B build -G Ninja
   cmake --build build -j
   ctest --test-dir build --output-on-failure
   ```

5. **Open PR:**
   - Use template checklist
   - Reference Action Plan items: `Relates: AP-RT-003`
   - CI runs automatically

6. **Resolve any ASK requests:**
   - Update `.codex/NEED-INFO/<file>.md` Resolution section
   - Apply solution
   - Remove or mark as RESOLVED

7. **Pass doc-gate:**
   - Update `docs/` if behavior changed
   - OR add `Relates: AP-XXX-NNN` to PR description

8. **Merge when:**
   - ✅ CI passes
   - ✅ No open ASK requests
   - ✅ Doc-gate satisfied (or advisory only)
   - ✅ Checklist complete

### Emergency Fix Flow

For critical hotfixes that need fast-track:

1. Create branch: `hotfix/critical-issue`
2. Make minimal fix
3. Add `skip-advisory` label to PR (if needed)
4. Get review approval
5. Merge when CI passes

### Metal Development Flow

1. Work on `feature/metal-apple-silicon` branch
2. Enable Metal in build:
   ```bash
   cmake -S . -B build -DWAYVERB_ENABLE_METAL=ON -DWAYVERB_STRICT_METAL=ON
   ```
3. Test on self-hosted runner via CI
4. Always maintain OpenCL fallback
5. Document in `docs/APPLE_SILICON.md`

## Monitoring and Maintenance

### Nightly Tasks (Automated)

- `archeology-xref.yml` runs at 03:17 UTC
- Updates cross-reference documentation
- Commits to main branch automatically

### Weekly Tasks (Manual)

- Review open ASK requests
- Triage Action Plan items
- Update status of BLOCKED items
- Check for stale PRs

### Monthly Tasks (Manual)

- Review and archive resolved ASK requests
- Update AGENTS.md for new patterns
- Audit CI efficiency and adjust detection logic
- Update material presets if new data available

## Troubleshooting

### CI Job Fails Unexpectedly

1. Check change detection:
   ```bash
   git diff --name-only origin/main...HEAD
   ```
2. Review job logs for actual failure
3. Run locally with same configuration
4. Check for environment-specific issues

### ASK Gate Blocks PR

1. List open requests:
   ```bash
   ls .codex/NEED-INFO/
   ```
2. For each file:
   - Read the question
   - Add answer to Resolution section
   - Mark as RESOLVED or remove file
3. Commit and push

### Doc Gate Advisory Noisy

- Add `Relates: AP-XXX-NNN` to PR description
- OR update relevant docs
- OR add `skip-doc-advisory` label (if truly not needed)

### Metal Smoke Test Fails

1. Check for fallback messages in log
2. Verify `WAYVERB_METAL=1` is set
3. Ensure self-hosted runner has Metal support
4. Check `docs/APPLE_SILICON.md` for known issues

## Configuration

### Repository Variables

- `DOC_ENFORCE=true`: Makes doc-gate blocking instead of advisory
- Set in GitHub Settings → Secrets and variables → Actions → Variables

### PR Labels

- `require-doc`: Makes doc-gate blocking for this PR
- `skip-advisory`: Skips non-essential advisory checks
- `needs-info`: Auto-applied by ASK system
- `ask`: Auto-applied by ASK system

## Integration Points

All systems integrate through:

1. **Git commits**: ASK requests, xref updates
2. **GitHub Actions**: Workflow triggers and status checks
3. **File system**: `.codex/`, `docs/`, `analysis/` directories
4. **Python scripts**: Shared utilities in `scripts/` and `tools/`

## Future Enhancements

Potential improvements:

- [ ] Web dashboard for Control Room status
- [ ] Slack/Discord notifications for ASK requests
- [ ] Automated performance regression detection
- [ ] Integration with JIRA/Linear for project management
- [ ] GitHub Actions bot for slash commands in comments
- [ ] Automated Action Plan item creation from TODOs

## See Also

- `.codex/POLICY_ASK.md` - Detailed ASK policy
- `docs/action_plan.md` - Current work tracking
- `docs/archeology.md` - Code analysis and TODO tracking
- `AGENTS.md` - Agent operating instructions
- `.github/workflows/README.md` - Workflow summary
