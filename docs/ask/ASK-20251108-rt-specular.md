# ASK – AP-RT-002 stochastic specular handling (2025-11-08)

**Question.** In Wayverb’s stochastic raytracer, shoebox scenes with scattering=0 were leaking
specular energy into the diffuse rain pipeline because `stochastic_path_info.physical` was being
accumulated even when `scatter_probability == 0`. Should the kernel zero/divert the accumulator in
specular-only scenarios to keep `raytracer_reverb.shoebox_ism_rt_parity` within ±0.5 dB?

---

**GPT‑5 Pro answer (verbatim).**

> Yes. In a purely specular scene (`scatter_probability == 0` at every hit), the
> stochastic/diffuse pipeline must receive **no energy**. Keep
> `stochastic_path_info.physical` at zero (for the diffuse side), or—if you want a
> single unified pass—route specular energy to a *specular* accumulator that is merged
> with the image‑source result (but never into the diffuse “rain”). Parity within ±0.5 dB
> is achieved when **only energy that has undergone at least one diffuse event** flows
> into the stochastic pipeline.
>
> ---
>
> ## Guiding principle (energy split at each hit)
>
> For a surface with absorption `α` and scattering `s`:
>
> * Diffuse (scattered) energy at the hit:
>   `E_diffuse = E_in * (1 - α) * s`
> * Specularly reflected throughput that continues:
>   `E_specular_next = E_in * (1 - α) * (1 - s)`
>
> When `s = 0`, `E_diffuse = 0` at every hit → the diffuse pipeline must remain silent.
>
> ---
>
> ## What to change
>
> ### 1) Gate writes into the diffuse accumulator
>
> Only add to the “physical” (diffuse) reservoir when there is *actual* scattering at
> the current interaction.
>
> ```cpp
> // before: spec_throughput holds energy arriving at this hit (already with distance/air losses)
> const float s = surface.scatter_probability;           // 0..1
> const float one_minus_alpha = 1.0f - surface.alpha;    // energy reflection (pre-scatter)
> const float diff = spec_throughput * one_minus_alpha * s;
>
> if (diff > 0.0f) {
>     // Deposit at the time-of-flight of THIS hit (first diffuse event point)
>     atomicAdd(&stochastic_path_info.physical[time_bin], diff);
>     path_info.has_scattered = true;  // mark that this path has entered the diffuse domain
> }
>
> // Specular continues
> spec_throughput *= one_minus_alpha * (1.0f - s);
> ```
>
> > With `s == 0`, `diff` is zero, so `stochastic_path_info.physical` never increases.
>
> ### 2) Enforce “no emit without scatter”
>
> Where you “emit as diffuse rain” later, require `has_scattered == true`. If you don’t
> track the boolean, compute it implicitly (e.g., keep a running
> `diffuse_accumulated` and check `> 0`):
>
> ```cpp
> if (!path_info.has_scattered) {
>     // this path never scattered → nothing to rain
>     return;
> }
> emit_diffuse_rain_from(path_info);
> ```
>
> ### 3) Optional: route specular to a deterministic/specular accumulator (but never to diffuse)
>
> If you want the stochastic kernel to also carry the specular solution (to avoid a
> separate pass), keep a **separate** specular accumulator:
>
> ```cpp
> specular_accum.add(receiver_bin, spec_throughput * coupling(receiver));
> ```
>
> Then either:
>
> * **Use only this specular_accum** for specular output (don’t also add ISM), or
> * **Prefer ISM** and **disable** specular accumulation in the stochastic pass to avoid double counting.
>
> The simplest way to pass your parity test is to **disable** any specular accumulation
> from the stochastic tracer when the ISM is active.
>
> ### 4) Fast-path short‑circuit (perf + safety)
>
> At scene setup time:
>
> ```cpp
> scene_has_any_scatter = any(surface.s > 0) || medium_scattering > 0;
>
> if (!scene_has_any_scatter) {
>     // Skip stochastic/diffuse tracing entirely, or make it a no-op write of zeros.
> }
> ```
>
> This guarantees zero diffuse output and avoids numerical dribble that can push you
> past ±0.5 dB.
>
> ---
>
> ## Common pitfalls that cause the leakage you’re seeing
>
> * **Accumulating specular throughput into `physical`** and converting later:
>   If you bank specular energy “to convert later,” you must multiply by `s` *at the
>   hit where the conversion is supposed to happen*. Deferring and then emitting
>   unconditionally will leak energy into diffuse when `s == 0`.
>
> * **Per-path final emission not conditioned on “has scattered”**:
>   Without the gate, purely specular paths appear as diffuse rain.
>
> * **Tiny nonzero `s` due to numeric noise**:
>   If `s` is computed, clamp very small values: `if (s <= 1e-7f) s = 0.0f;`
>
> ---
>
> ## Reference flow (specular-first, then diffuse)
>
> 1. **Launch ray** with `spec_throughput = E_src`, `has_scattered = false`.
> 2. **Iterate hits**:
>
>    * Compute `s` and `α`.
>    * **Deposit diffuse**: `diff = spec_throughput*(1-α)*s` → add to `physical` at this hit’s time bin; set `has_scattered = true`.
>    * **Update specular throughput**: `spec_throughput *= (1-α)*(1-s)`.
>    * Break if `spec_throughput` falls below epsilon or bounce limit reached.
> 3. **Emit**:
>
>    * If `has_scattered`: run your diffusion/rain logic using the `physical` reservoir.
>    * Else: do **nothing** for diffuse. (Specular energy is either handled by ISM or a separate specular accumulator.)
>
> This split is unbiased, energy-conserving, and yields exactly zero diffuse energy
> when `s == 0`, so the image‑source and stochastic totals agree to within the ±0.5 dB
> tolerance (remaining differences will be floating‑point noise only).
>
> ---
>
> ## Minimal patch checklist
>
> * [ ] Add `bool has_scattered` (or equivalent) to `stochastic_path_info`.
> * [ ] Write to `stochastic_path_info.physical` **only** with factor `*(1-α)*s` at a hit.
> * [ ] Multiply specular continuation by `*(1-α)*(1-s)`; never store this into the diffuse reservoir.
> * [ ] Gate the “emit as diffuse rain” stage on `has_scattered`.
> * [ ] Optionally bypass stochastic entirely when `scene_has_any_scatter == false`.
> * [ ] In parity test, ensure the stochastic/specular channel is **disabled** if you are also summing the ISM result.
>
> Implementing the above keeps specular‑only energy out of the diffuse path and
> preserves ISM parity within ±0.5 dB.
*** End Patch
PATCH
