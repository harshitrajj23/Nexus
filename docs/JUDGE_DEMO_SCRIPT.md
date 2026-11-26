# NEXUS Presenter Guide & 90-Second Demo Script

This document provides the exact word-for-word spoken narration, physical UI actions, and timing benchmarks for presenting NEXUS to hackathon judges.

---

## 📋 Presenter Pre-Flight Checklist (Do this 2 minutes before presenting)

1. Run the automated preflight check:
   ```bash
   ./scripts/judge_preflight.sh
   ```
   Ensure all critical checks return `[PASS]`.
2. Start the FastAPI backend:
   ```bash
   source .venv/bin/activate
   uvicorn agent_orchestrator.api:app --host 0.0.0.0 --port 8000
   ```
3. Start the React dashboard:
   ```bash
   cd dashboard && npm run dev
   ```
   Open `http://localhost:5173` in your browser.

---

## 🎙️ Spoken Narration Script (90 Seconds)

### [00:00 - 00:15] 1. The Hook & The Problem
> **Presenter:**  
> "Autonomous AI agents don't compromise infrastructure using traditional malware—they use the tools we give them. In isolation, every tool invocation looks benign: reading a file, running an internal database query, or making an API call. But when chained chronologically, individually legitimate tool calls combine into an unauthorized multi-step attack trajectory."

### [00:15 - 00:30] 2. Why Isolated Tool-Call Monitoring Fails
> **Presenter:**  
> "Current security guardrails evaluate tool calls in a vacuum. When an agent requests an outbound HTTP POST, the guardrail approves it because the tool is permitted. The gateway misses what happened two steps earlier: that same agent opened `credentials/config.env`, escalated its IAM role to `ADMIN`, and is now exfiltrating those stolen keys. Without time-aware relational context, isolated monitoring is completely blind."

### [00:30 - 00:55] 3. Running the Live Attack Demo (Experiment B)
> **Presenter Action:**  
> *Click "Experiment Lab" &rarr; Click "Run Attack Scenario" (or run `./scripts/judge_demo.sh` in terminal).*  
>  
> **Presenter Spoken:**  
> "Let's watch NEXUS monitor this agent in real time across four chronological steps:  
> - **Step 1:** The agent inspects `docs/spec.md`. Evidence score: 10/100, policy remains **Normal**.  
> - **Step 2:** It reads `credentials/config.env`. The evidence score jumps to 30/100, elevating the policy to **Suspicious**.  
> - **Step 3:** It escalates its role to `ADMIN`. Score rises to 55/100, triggering a **HighRisk** policy.  
> - **Step 4:** It attempts an external dump to an unapproved endpoint. The score surges to **95/100**, and the policy recommendation escalates to **CRITICAL: Recommend workflow pause and investigation**."

### [00:55 - 01:15] 4. Inspecting the Causal Graph & Evidence Path
> **Presenter Action:**  
> *Navigate to "Live Graph" tab and click the red animated attack path.*  
>  
> **Presenter Spoken:**  
> "Under the hood, NEXUS constructs a dynamic heterogeneous graph in native C++17 LibTorch, processing each event in under 5 milliseconds. On this canvas, you see the reconstructed causal attack path linking the compromised agent through the sensitive config file and elevated role directly to the external egress destination. Notice the distinct semantic tags: `[SENSITIVE]`, `[ELEVATED]`, and `[TARGET EGRESS]`."

### [01:15 - 01:30] 5. The Four-Layer Framework & Exact Closing Sentence
> **Presenter Action:**  
> *Point to the Four-Layer Decision Framework on the Active Decision Panel.*  
>  
> **Presenter Spoken:**  
> "NEXUS strictly separates four layers: the neural model signal, the deterministic evidence score, the policy recommendation, and the enforcement status—honestly marked as *Advisory only* because no blocking daemon was configured.  
>  
> **NEXUS does not claim that a black-box model blocked an agent. It provides time-aware evidence so security teams can make a defensible intervention.**"

---

## 💡 Quick Reference: Tab-by-Tab Judge Walkthrough

| Tab | What Judges See | Key Talking Point |
|---|---|---|
| **Overview** | Executive Header, "Why NEXUS?" comparison, Active Security Decision panel, KPI metrics. | Answers the 5 core questions in under 10 seconds with zero visual clutter. |
| **Live Graph** | Directed heterogeneous graph, timeline scrubber, animated red attack edges, semantic chips. | Reconstructs multi-hop attack trajectories; interactive time slider demonstrates causal progression. |
| **Threat Intel** | Chronological hops table, attribution weights ($a_e$), structural multipliers (2.5x, 2.0x, 3.0x), human narrative. | Complete explainability for security analysts with transparent mathematical attribution. |
| **Model Intel** | Confusion matrix, training loss curves, and empirical ablation table comparing temporal vs static models. | Proof of why temporal modeling is mandatory: static graph models suffered a 100% False Negative Rate. |
| **Experiment Lab** | Live runner for Experiments A (Normal), B (Adversarial Exfiltration), and C (Held-Out Unseen). | Evaluators can trigger any scenario with one click and observe live risk scoring and latency. |
| **Intent & AgntID** | Intent vs Runtime Decision Comparison table and Gap Analysis panel. | Evaluates identical tool calls on config.env under approved rotation vs exfiltration intent; honest AgntID status. |
