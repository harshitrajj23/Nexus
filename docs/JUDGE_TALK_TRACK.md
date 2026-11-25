# NEXUS Hackathon Judge Presentation Talk Track

**Session Target:** 90 Seconds  
**Track:** AI Agent Security & Runtime Threat Intelligence (AgntID Challenge)  
**Core Thesis:** *Autonomous AI agents perform sequences of individually legitimate tool invocations that collectively construct unauthorized attack trajectories.*

---

## ⏱️ The 90-Second Timed Script

```
[00:00 - 00:15] SECTION A: THE 10-SECOND HOOK & PROBLEM
"Autonomous AI agents don't break into systems using malware—they use the tools we give them.
An agent reading a documentation file, querying an internal database, or making a web request
looks completely harmless in isolation. But when chained chronologically, individually legitimate
tool calls become a dangerous multi-step attack trajectory."
```

```
[00:15 - 00:30] SECTION B: WHY ISOLATED TOOL MONITORING FAILS
"Traditional security gateways evaluate tool executions in a vacuum: 'Is this agent allowed to
make an HTTP POST?' The gateway says 'Yes.'
What it completely misses is what happened two steps earlier: that same agent opened a secret
credential file, escalated its IAM role to ADMIN, and is now uploading those stolen tokens to an
external server. Without time-aware relational context, isolated monitoring is blind."
```

```
[00:30 - 00:55] SECTION C: LIVE DEMO & RISK ESCALATION
[ACTION: Trigger Experiment B via ./scripts/judge_demo.sh or in the Experiment Lab]
"Watch what happens live as NEXUS monitors this agent across 4 chronological steps:
- Step 1: Benign recon on spec.md -> Evidence Score: 10/100, Policy: Normal.
- Step 2: The agent accesses credentials/config.env -> Score jumps to 30/100, Policy: Suspicious (+25 points).
- Step 3: It escalates role to ADMIN -> Score rises to 55/100, Policy: HighRisk (+25 points).
- Step 4: It initiates an outbound POST to an external endpoint -> Score surges to 95/100, Policy: CRITICAL.
Notice how the policy recommendation immediately updates to:
'Recommend workflow pause and investigation'."
```

```
[00:55 - 00:75] SECTION D: TECHNICAL CREDIBILITY & CAUSAL GRAPH
[ACTION: Switch to Live Graph and Threat Intel tab]
"Under the hood, NEXUS maintains a dynamic heterogeneous graph across 7 entity types and
7 relation types in native C++17 LibTorch, processing each event in under 5 milliseconds.
In our ablation study, a static graph snapshot failed completely with a 100% False Negative Rate.
Adding GRU temporal recurrence eliminated that blindspot.
On screen, you can see our multi-hop causal DFS tracing the exact path from the compromised agent
through the stolen credential and elevated role to the unauthorized external destination."
```

```
[00:75 - 00:90] SECTION E: THE FOUR-LAYER FRAMEWORK & CLOSING
"NEXUS strictly separates four layers:
1. Neural Model Signal: Raw probability from the LibTorch R-GCN + GRU.
2. Deterministic Evidence Score: Clear points based on verified graph topology.
3. Policy Recommendation: Actionable guidance for human incident response.
4. Enforcement Status: Transparently marked as 'Advisory only — no live blocking gateway attached'.

NEXUS does not claim that a black-box model blocked an agent. It provides time-aware evidence so security teams can make a defensible intervention."
```

---

## ❓ Likely Judge Questions & Honest Answers

### Q1: Why not rely on static rule-based detection alone?
> **Answer:** Static rules evaluate predefined signatures and fail when attack steps are interleaved with benign work or span arbitrary relational hops (e.g. agent &rarr; tool &rarr; intermediate file &rarr; bash pipe &rarr; network destination). Relational Graph Convolutional Networks (R-GCN) compress complex multi-entity relationships into semantic embeddings, allowing temporal GRUs to detect multi-step progression even when tools or sequence lengths vary.

### Q2: Why is a heterogeneous graph necessary?
> **Answer:** Autonomous agents operate in a graph-structured domain: an `Agent` uses a `Tool`, accesses a `Resource`, is authorized by a `Permission`, assumes an `Identity`, and connects to a `Destination`. Modeling these as distinct typed nodes and relations allows causal DFS backtracking ($t_k \ge t_{k-1}$) to reconstruct verifiable attack paths rather than guessing correlations.

### Q3: Why is temporal modeling essential? What did your ablation prove?
> **Answer:** Our ablation benchmark (`data/metrics.json`) compared a non-temporal static R-GCN model against our temporal R-GCN + GRU model. The static model suffered a catastrophic **100.0% False Negative Rate**—predicting Normal for every attack sequence because isolated snapshots lack chronological ordering. Adding GRU recurrence dropped the False Negative Rate by **80.0 percentage points** (FNR = 20.0%) with minimal latency overhead ($\approx 3.16\text{ ms}$).

### Q4: Why is the neural model not the sole decision-maker?
> **Answer:** In high-stakes enterprise security, relying solely on black-box neural classifications leads to hallucinations, uncalibrated confidence, and alert fatigue. Furthermore, on small discrete benchmark splits, 4-class neural heads can exhibit severity clustering. NEXUS solves this by pairing the neural representation with transparent, deterministic graph-topology evidence scoring. Every Critical alert has inspectable physical proof.

### Q5: Is the live demonstration safe?
> **Answer:** Completely safe. All agent workflows execute in temporary sandboxes in `/tmp` using mock endpoints (`mock://`). No real network requests, sensitive tokens, or destructive commands are ever run.

### Q6: Is AgntID actually enforcing blocks in this demo?
> **Answer:** No. We honestly report AgntID status as `NOT_EVALUATED` (`enforced: false`) because the official AgntID runtime SDK was unconfigured in this host environment. We reject fabricating fake blocks; NEXUS provides advisory intelligence designed to feed into an enforcement gateway.
