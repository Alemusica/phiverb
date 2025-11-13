# GitHub Actions Workflows

## Active Workflows (4 total)

### Essential (Always Run)

1. **ci-build-test.yml** - Main CI Pipeline ✅
   - Triggers: PR to main, detects changed components
   - Jobs:
     - `detect`: Determines which components changed (RT/DWM/Metal)
     - `rt-tests`: Runs raytracer tests if RT code changed
     - `dwm-tests`: Runs waveguide tests if DWM code changed
     - `regression-test`: Runs apple_silicon_regression on all PRs to detect NaN/Inf/silence
     - `metal-smoke`: Tests Metal backend on self-hosted macOS ARM64 runner
   - Status: ✅ Essential - keeps

2. **ask-gate.yml** - ASK Request Checker ✅
   - Triggers: PR to main
   - Checks for open ASK requests
   - Status: ✅ Essential - keeps

### Advisory (Non-Blocking)

3. **archeology-xref.yml** - Auto Documentation Update ⚠️
   - Triggers: PR, push to main, nightly schedule
   - Auto-generates cross-reference docs (comment_summary, archeology, action_plan)
   - Status: ⚠️ Advisory - keeps for auto-docs

4. **doc-gate.yml** - Documentation Gate ⚠️
   - Triggers: PR to main
   - Checks if docs updated or Action Plan cited
   - Status: ⚠️ Advisory (non-blocking unless `require-doc` label)

## Removed Workflows (4 total)

- **metal-ci.yml** ❌ - Duplicated ci-build-test.yml with incorrect CMake flags
- **agent-iteration-router.yml** ❌ - Noisy, routes "RT said:" / "DWM said:" comments
- **progress-advisor.yml** ❌ - Redundant with doc-gate.yml
- **doc-consistency.yml** ❌ - Redundant dry-run of archeology-xref.yml

## Summary

**Before:** 8 workflows (metal-ci was broken, 5 advisory)  
**After:** 4 workflows (2 essential CI + 2 advisory non-blocking)  
**Reduction:** 50% fewer workflows, cleaner CI dashboard

## Configuration

All advisory workflows are non-blocking by default and don't fail PRs.

To make doc-gate blocking, add the `require-doc` label to a PR or set repository variable `DOC_ENFORCE=true`.
