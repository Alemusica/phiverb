# Wayverb Infrastructure Guide

This document provides a comprehensive overview of the Wayverb development infrastructure, consolidating features from PRs #1, #5, #6, and #8.

## Quick Reference

| Component | Purpose | Location | Status |
|-----------|---------|----------|--------|
| Split CI | RT/DWM/Metal tests | `.github/workflows/ci-build-test.yml` | ✅ Active |
| Doc Gate | Advisory doc checks | `.github/workflows/doc-gate.yml` | ✅ Active |
| ASK Gate | Block on questions | `.github/workflows/ask-gate.yml` | ✅ Active |
| Auto XRef | Nightly doc sync | `.github/workflows/archeology-xref.yml` | ✅ Active |
| ASK System | Question tracking | `scripts/ask/`, `.codex/` | ✅ Active |
| Action Plan | Work tracking | `docs/action_plan.md` | ✅ Active |
| PR Template | Standard checklist | `.github/pull_request_template.md` | ✅ Active |
| Control Room | Command patterns | `docs/CONTROL_ROOM.md` | ✅ Documented |

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     GitHub Pull Request                      │
└────────────┬────────────────────────────────────────────────┘
             │
             ├─► PR Template Applied (checklist)
             │
             ├─► CI Workflows Triggered:
             │   │
             │   ├─► Detect: Analyze changed files
             │   │   └─► Determine: RT? DWM? Metal?
             │   │
             │   ├─► RT Tests (if raytracer/* changed)
             │   │   ├─► Build with Ninja
             │   │   ├─► CTest unit tests
             │   │   └─► QA validation suite
             │   │
             │   ├─► DWM Tests (if waveguide/* changed)
             │   │   ├─► Build with Ninja
             │   │   ├─► Passivity/CFL tests
             │   │   └─► QA validation suite
             │   │
             │   └─► Metal Smoke (if metal/* changed)
             │       ├─► Build with Metal flags
             │       ├─► Run on ARM64 runner
             │       └─► Verify no fallback
             │
             ├─► Ask Gate (BLOCKING)
             │   ├─► Scan .codex/NEED-INFO/
             │   └─► FAIL if unresolved questions exist
             │
             ├─► Doc Gate (ADVISORY)
             │   ├─► Check docs/* changes
             │   ├─► Look for AP-XXX-NNN references
             │   └─► WARN if neither present
             │
             └─► Archeology XRef (INFORMATIONAL)
                 ├─► Generate comment_summary.md
                 ├─► Update archeology.md
                 ├─► Refresh action_plan.md
                 └─► Post summary in PR comment

┌─────────────────────────────────────────────────────────────┐
│                      Nightly Schedule                        │
└────────────┬────────────────────────────────────────────────┘
             │
             └─► Archeology XRef
                 ├─► Scan all TODO comments
                 ├─► Cross-reference with action plan
                 ├─► Generate updated docs
                 └─► Auto-commit to main
```

## PR #1: CI Split + Metal Smoke + Doc Advisory + Token Guard

### Status: ✅ **COMPLETE**

### Components

#### 1. Split CI Pipeline

**File:** `.github/workflows/ci-build-test.yml`

**Features:**
- Smart change detection using git merge-base
- Conditional job execution (skip unaffected components)
- Three specialized test jobs:
  - `rt-tests`: Raytracer validation
  - `dwm-tests`: Waveguide passivity/CFL
  - `metal-smoke`: Metal backend verification

**Benefits:**
- Faster CI (only run affected tests)
- Better resource utilization
- Clear test isolation

#### 2. Metal Smoke Test

**Location:** `metal-smoke` job in `ci-build-test.yml`

**Features:**
- Runs on self-hosted ARM64 runner
- Builds with `-DWAYVERB_ENABLE_METAL=ON`
- Verifies no fallback to OpenCL
- Strict mode: fails on any fallback

**Validation:**
- Checks JSON output for `backend: metal`
- Scans logs for fallback messages
- Exits with error code 2 if fallback detected

#### 3. Doc Advisory Gate

**File:** `.github/workflows/doc-gate.yml`

**Features:**
- Checks if docs updated OR AP item referenced
- Advisory by default (doesn't block)
- Can be made blocking with:
  - `require-doc` label on PR
  - Repository variable `DOC_ENFORCE=true`

**Acceptance criteria:**
- Docs touched: `docs/` files changed
- AP cited: PR body contains `AP-XXX-NNN`

#### 4. Token Guard (ASK System)

**Files:**
- `.github/workflows/ask-gate.yml`
- `scripts/ask/new`
- `scripts/ask/gate.py`
- `.codex/POLICY_ASK.md`

**Features:**
- BLOCKING workflow that fails if open questions exist
- Questions stored in `.codex/NEED-INFO/`
- Automatic issue creation via `gh` CLI
- Resolution tracking

**Workflow:**
1. Developer encounters design question
2. Creates ASK request: `./scripts/ask/new`
3. File created in `.codex/NEED-INFO/`
4. GitHub issue opened automatically
5. PR merge BLOCKED until resolved
6. Maintainer provides answer in Resolution section
7. Developer applies solution
8. File removed or marked RESOLVED
9. PR unblocked

## PR #5: Agent Iteration Router + Action Plan Doc

### Status: ✅ **COMPLETE**

### Components

#### 1. Action Plan Document

**File:** `docs/action_plan.md`

**Structure:**
```markdown
## 1. Raytracer — energia e diffuse rain

**File principali**
- path/to/file1.h
- path/to/file2.cpp

**Checklist**
- [x] Item 1 completed
- [ ] Item 2 in progress

**Verifiche**
- Test criteria
- Acceptance conditions
```

**Features:**
- Organized by technical cluster
- Tracks completion status
- Links to relevant files
- Defines acceptance criteria
- Uses unique IDs: `AP-RT-001`, `AP-DWM-002`, etc.

#### 2. Agent Iteration Router (Archeology XRef)

**File:** `.github/workflows/archeology-xref.yml`

**Features:**
- Runs on PR events and nightly schedule
- Executes `tools/docs_refresh.py`
- Generates three documents:
  - `docs/archeology.md`: Current state and TODO tracking
  - `docs/action_plan.md`: Work item tracking
  - `analysis/comment_summary.md`: Comment inventory

**Auto-commit behavior:**
- On `push` events: commits to main branch
- On `pull_request`: posts summary comment
- Timestamp tracked: `<!--XREF-DATE-->YYYY-MM-DD HH:MM:SS<!--/XREF-DATE-->`

#### 3. Documentation Sync

**Tool:** `tools/docs_refresh.py`

**Scans:**
- TODO comments in source code
- Action plan items
- Cross-references between them
- ASK requests (if present)

**Generates:**
- Markdown tables of TODOs by file
- Status updates for action items
- Comment inventory statistics

## PR #6: Progress Advisor + PR Template (AP Checklists)

### Status: ✅ **COMPLETE**

### Components

#### 1. PR Template

**File:** `.github/pull_request_template.md`

**Format:**
```markdown
### Scopo
- [ ] RT (Raytracer)
- [ ] DWM (Digital Waveguide Mesh)
- [ ] Shared

### Checklist
- [ ] Build & test verdi
- [ ] scripts/check_scope.py OK
- [ ] QA suite pass (EDC/T20/T30, bounds)
- [ ] Niente refactor fuori scope
```

**Purpose:**
- Immediate scope visibility
- Standard verification steps
- Prevent scope creep
- Integrate with doc-gate

#### 2. Progress Advisor (Doc Gate)

Already covered in PR #1, but specifically:

**Advisory Messages:**
```
::notice::Doc advisory: aggiungi 'Relates: AP-...' o 
         aggiorna docs/ se utile. (Consulta, non blocca)
```

**Enforcement Mode:**
```
::error::Doc advisory: ... — enforced (label 'require-doc' 
        o DOC_ENFORCE=true)
```

#### 3. Scope Checker

**File:** `scripts/check_scope.py`

**Usage:**
```bash
python3 scripts/check_scope.py
```

**Checks:**
- Files changed match declared scope
- No unexpected modifications outside scope
- Follows project conventions

## PR #8: Control Room (Slash Commands + Agent Runner)

### Status: ✅ **COMPLETE**

### Components

#### 1. Control Room Documentation

**File:** `docs/CONTROL_ROOM.md` (created in this PR)

**Contents:**
- Complete infrastructure guide
- Slash command patterns
- Workflow examples
- Troubleshooting guide
- Integration points

#### 2. Slash Command Patterns

Documented patterns for common operations:

| Command | Script | Purpose |
|---------|--------|---------|
| `/ask` | `scripts/ask/new` | Create ASK request |
| `/scope` | `scripts/check_scope.py` | Verify PR scope |
| `/monitor` | `scripts/monitor_app_log.sh` | Watch logs |
| `/qa` | `scripts/qa/run_validation_suite.py` | Run QA |
| `/regression` | `tools/run_regression_suite.sh` | Full regression |
| `/xref` | `tools/docs_refresh.py` | Update docs |
| `/inventory` | `tools/comment_inventory_diff.py` | Check comments |

#### 3. Agent Runner (Custom Agents)

**File:** `.github/agents/phiverb-metal-assistant.agent.md`

**Integration:**
- Custom agent for Metal/Apple Silicon work
- Invoked via GitHub Copilot CLI
- Specialized in OpenCL→Metal conversion
- Integrated with ASK system for blockers

**Usage:**
```bash
gh copilot chat -a .github/agents/phiverb-metal-assistant.agent.md \
  "How do I fix clBuildProgram error?"
```

## Integration Examples

### Example 1: Adding a New Feature

```bash
# 1. Create branch
git checkout -b feature/brdf-improvement

# 2. Make changes to src/raytracer/src/cl/brdf.cpp

# 3. Check scope
python3 scripts/check_scope.py

# 4. If unclear on formula, create ASK
./scripts/ask/new \
  --title "ASK: Correct BRDF normalization factor" \
  --question "Should Lambert BRDF use 1/π or cos(θ)/π?" \
  --topic "rt"

# 5. Wait for resolution in .codex/NEED-INFO/<file>.md

# 6. Apply solution and run tests
cmake -S . -B build -G Ninja
cmake --build build -j
ctest --test-dir build --output-on-failure

# 7. Run QA
python3 scripts/qa/run_validation_suite.py \
  --cli build/bin/wayverb_cli --scenes tests/scenes

# 8. Update action plan
# Edit docs/action_plan.md, mark item as complete

# 9. Create PR with template
# Reference: "Relates: AP-RT-003"

# 10. Address CI feedback

# 11. Resolve ASK by updating Resolution section
# Remove .codex/NEED-INFO/<file>.md

# 12. Merge when all checks pass
```

### Example 2: Nightly Documentation Sync

```bash
# Runs automatically at 03:17 UTC via archeology-xref.yml

# Manual trigger:
python3 tools/docs_refresh.py

# Generates:
# - docs/archeology.md (updated TODO list)
# - docs/action_plan.md (refreshed status)
# - analysis/comment_summary.md (comment stats)

# Auto-commits to main:
git add docs/archeology.md docs/action_plan.md analysis/comment_summary.md
git commit -m "docs: auto-xref (nightly/main)"
git push
```

### Example 3: Metal Development Workflow

```bash
# 1. Work on feature branch
git checkout feature/metal-apple-silicon

# 2. Build with Metal
cmake -S . -B build -G Ninja \
  -DWAYVERB_ENABLE_METAL=ON \
  -DWAYVERB_STRICT_METAL=ON

cmake --build build -j

# 3. Test locally
WAYVERB_METAL=1 build/bin/apple_silicon_regression

# 4. Monitor logs
scripts/monitor_app_log.sh &
WAYVERB_METAL=1 tools/run_wayverb.sh

# 5. Create PR (triggers metal-smoke on self-hosted runner)

# 6. CI verifies no OpenCL fallback

# 7. Update docs/APPLE_SILICON.md with findings

# 8. Merge when metal-smoke passes
```

## Maintenance

### Daily
- Monitor CI job health
- Review open ASK requests
- Triage new issues

### Weekly
- Review Action Plan progress
- Update stale ASK requests
- Check for CI optimization opportunities

### Monthly
- Archive resolved ASK requests
- Update infrastructure docs
- Review and prune old branches
- Audit workflow efficiency

### Quarterly
- Major version planning
- Infrastructure improvements
- Performance baseline updates
- Material database refresh

## Metrics and Health

### CI Health Indicators

**Green (Healthy):**
- Average CI time < 10 minutes
- Pass rate > 95%
- No flaky tests

**Yellow (Attention Needed):**
- Average CI time 10-20 minutes
- Pass rate 85-95%
- 1-2 flaky tests

**Red (Action Required):**
- Average CI time > 20 minutes
- Pass rate < 85%
- 3+ flaky tests

### ASK System Health

**Green:**
- Average resolution time < 48 hours
- < 3 open requests at any time

**Yellow:**
- Average resolution time 48-96 hours
- 3-5 open requests

**Red:**
- Average resolution time > 96 hours
- > 5 open requests (indicates documentation gap)

### Doc Sync Health

**Green:**
- Archeology XRef runs successfully daily
- < 10% comment drift

**Yellow:**
- Occasional XRef failures
- 10-25% comment drift

**Red:**
- Frequent XRef failures
- > 25% comment drift (needs manual intervention)

## Future Roadmap

### Phase 1 (Current) - ✅ COMPLETE
- [x] Split CI pipeline
- [x] Metal smoke tests
- [x] ASK system
- [x] Doc advisory
- [x] Action plan tracking
- [x] PR templates
- [x] Control Room documentation

### Phase 2 (Next)
- [ ] Performance regression detection
- [ ] Automated material updates
- [ ] Enhanced Metal testing
- [ ] FOA export validation
- [ ] Listening task automation

### Phase 3 (Future)
- [ ] Web dashboard for Control Room
- [ ] Real-time monitoring
- [ ] Predictive failure analysis
- [ ] Automated A/B testing for algorithms
- [ ] Integration with external issue trackers

## Support and Troubleshooting

### Getting Help

1. **Check documentation:**
   - This file for infrastructure overview
   - `docs/CONTROL_ROOM.md` for detailed commands
   - `docs/action_plan.md` for current work
   - `.codex/POLICY_ASK.md` for ASK system

2. **Create ASK request:**
   - For technical questions
   - For design decisions
   - For blocked work

3. **Check GitHub Actions logs:**
   - For CI failures
   - For workflow issues

4. **Review recent commits:**
   - `git log --oneline --grep="infra\|docs:"`
   - Check for related changes

### Common Issues

See `docs/CONTROL_ROOM.md` § Troubleshooting for detailed solutions.

## References

- **Workflows:** `.github/workflows/README.md`
- **Agents:** `.github/agents/README.md`, `AGENTS.md`
- **ASK Policy:** `.codex/POLICY_ASK.md`
- **Control Room:** `docs/CONTROL_ROOM.md`
- **Action Plan:** `docs/action_plan.md`
- **Archeology:** `docs/archeology.md`

## Changelog

### 2025-11-13: Infrastructure Consolidation (This PR)
- Documented all infrastructure components from PRs #1, #5, #6, #8
- Created `docs/CONTROL_ROOM.md` with detailed command reference
- Created `docs/INFRASTRUCTURE.md` (this file)
- Verified all workflows are functional
- Confirmed ASK system is operational
- Validated Action Plan tracking is active

### Previous
- See individual commits in git history
- PR references: #1, #5, #6, #8
