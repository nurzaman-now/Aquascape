# Graph Report - Aquascape  (2026-11-16)

## Corpus Check
- Corpus is ~7,887 words - fits in a single context window. You may not need a graph.

## Summary
- 73 nodes · 151 edges · 6 communities (5 shown, 1 thin omitted)
- Extraction: 82% EXTRACTED · 18% INFERRED · 0% AMBIGUOUS · INFERRED: 27 edges (avg confidence: 0.85)
- Token cost: 1,200 input · 450 output

## Community Hubs (Navigation)
- Telegram Bot Interface
- Scheduler & Rule Enforcement
- WiFi Captive Portal
- Actuators & Servo PWM Control
- System Architecture & Features

## God Nodes (most connected - your core abstractions)
1. `handleTelegramMessages()` - 21 edges
2. `formatTime()` - 11 edges
3. `handleLampWizardInput()` - 9 edges
4. `executeSelectLampColor()` - 8 edges
5. `sendHelpMenu()` - 8 edges
6. `finalizeLampSchedule()` - 7 edges
7. `handleFeedWizardInput()` - 7 edges
8. `IoT Smart Aquascape Controller (ESP32)` - 7 edges
9. `parseHoursMinutes()` - 6 edges
10. `formatLampSessionList()` - 6 edges

## Surprising Connections (you probably didn't know these)
- `handleTelegramMessages()` --calls--> `setBubbleLevel()`  [INFERRED]
  TelegramBot.h → Actuators.h
- `handleTelegramMessages()` --calls--> `setFeedPortion()`  [INFERRED]
  TelegramBot.h → Actuators.h
- `handleTelegramMessages()` --calls--> `turnLampOffManual()`  [INFERRED]
  TelegramBot.h → Actuators.h
- `handleTelegramMessages()` --calls--> `getPortionName()`  [INFERRED]
  TelegramBot.h → Actuators.h
- `processFeeding()` --calls--> `feedFish()`  [INFERRED]
  Scheduler.h → Actuators.h

## Import Cycles
- None detected.

## Communities (6 total, 1 thin omitted)

### Community 0 - "Telegram Bot Interface"
Cohesion: 0.38
Nodes (13): formatFeedScheduleList(), formatTime(), askLampConfirmation(), askLampOffConfirmation(), executeSelectLampColor(), finalizeFeedSchedule(), finalizeLampSchedule(), handleFeedWizardInput() (+5 more)

### Community 1 - "Scheduler & Rule Enforcement"
Cohesion: 0.26
Nodes (12): feedFish(), applyFeedSchedule(), applyLampSchedule(), calculateDurationMinutes(), calculateTotalLampDuration(), checkDayChange(), formatLampSessionList(), getNextFeedingSchedule() (+4 more)

### Community 2 - "WiFi Captive Portal"
Cohesion: 0.22
Nodes (5): closeModal(), loadInfo(), openManualModal(), openModalByIndex(), scanWifi()

### Community 3 - "Actuators & Servo PWM Control"
Cohesion: 0.20
Nodes (6): getPortionName(), setBubbleLevel(), setFeedPortion(), setLampColor(), turnLampOffManual(), turnLampOnManual()

### Community 5 - "System Architecture & Features"
Cohesion: 0.25
Nodes (8): Adaptive Fish Feeding & Overfeeding Protection, Bubble Filter Flow Control, Dynamic Light Scheduling & Algae Protection, IoT Smart Aquascape Controller (ESP32), NVS Flash Lifespan Preservation, Offline Autonomy, Telegram Bot Control, WiFi Captive Portal

## Knowledge Gaps
- **7 isolated node(s):** `Telegram Bot Control`, `Dynamic Light Scheduling & Algae Protection`, `Adaptive Fish Feeding & Overfeeding Protection`, `Bubble Filter Flow Control`, `WiFi Captive Portal` (+2 more)
  These have ≤1 connection - possible missing edges. (Counts symbols only; 28 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **1 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `handleTelegramMessages()` connect `Telegram Bot Interface` to `Scheduler & Rule Enforcement`, `Actuators & Servo PWM Control`?**
  _High betweenness centrality (0.087) - this node is a cross-community bridge._
- **Are the 12 inferred relationships involving `handleTelegramMessages()` (e.g. with `getPortionName()` and `setBubbleLevel()`) actually correct?**
  _`handleTelegramMessages()` has 12 INFERRED edges - model-reasoned connections that need verification._
- **What connects `Telegram Bot Control`, `Dynamic Light Scheduling & Algae Protection`, `Adaptive Fish Feeding & Overfeeding Protection` to the rest of the system?**
  _7 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Why does `getPortionName()` connect `Actuators & Servo PWM Control` to `Telegram Bot Interface`?**
  _High betweenness centrality (0.025) - this node is a cross-community bridge._
- **Are the 5 inferred relationships involving `formatTime()` (e.g. with `finalizeFeedSchedule()` and `finalizeLampSchedule()`) actually correct?**
  _`formatTime()` has 5 INFERRED edges - model-reasoned connections that need verification._
- **Are the 4 inferred relationships involving `handleLampWizardInput()` (e.g. with `calculateDurationMinutes()` and `formatTime()`) actually correct?**
  _`handleLampWizardInput()` has 4 INFERRED edges - model-reasoned connections that need verification._