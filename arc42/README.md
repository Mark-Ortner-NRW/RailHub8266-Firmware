# RailHub8266 Firmware - Architecture Documentation

This directory contains the complete architecture documentation for the **RailHub8266 ESP8266 Model Railway Controller** firmware, following the arc42 template.

## Project Overview

**RailHub8266** is an ESP8266-based WiFi-controlled PWM output controller designed for model railways and lighting control systems. It provides 7 PWM outputs with real-time WebSocket updates, chasing light groups (exclusive to ESP8266 version), and persistent configuration storage.

- **Device**: ESP8266 (ESP-12E compatible)
- **Version**: 2.0.0
- **Repository**: RailHub8266-Firmware
- **Owner**: Mark-Ortner-NRW

## Documentation Structure

This documentation follows the arc42 template structure:

### 1. [Introduction and Goals](01_introduction_and_goals.md)
Requirements overview, stakeholder overview, and quality goals.

### 2. [Architecture Constraints](02_architecture_constraints.md)
Technical and organizational constraints affecting the architecture.

### 3. [System Scope and Context](03_system_scope_and_context.md)
Business and technical context, external interfaces.

### 4. [Solution Strategy](04_solution_strategy.md)
Fundamental decisions and solution approaches.

### 5. [Building Block View](05_building_block_view.md)
Static decomposition of the system (code structure).

### 6. [Runtime View](06_runtime_view.md)
Runtime behavior of important scenarios.

### 7. [Deployment View](07_deployment_view.md)
Technical infrastructure and hardware deployment.

### 8. [Crosscutting Concepts](08_crosscutting_concepts.md)
Overall, principal regulations and solution ideas.

### 9. [Architecture Decisions](09_architecture_decisions.md)
Important architecture decisions (ADR format).

### 10. [Quality Requirements](10_quality_requirements.md)
Quality tree and quality scenarios.

### 11. [Risks and Technical Debt](11_risks_and_technical_debt.md)
Known risks, technical debt, and open issues.

### 12. [Glossary](12_glossary.md)
Important domain and technical terms.

## Key Features

- **7 PWM Outputs**: Individual control with brightness (0-100%)
- **WebSocket Server**: Real-time status updates every 500ms
- **Chasing Light Groups**: Up to 4 sequential lighting effects (ESP8266-exclusive)
- **Blink Intervals**: Per-output configurable blinking (0-65535ms)
- **WiFiManager Integration**: Easy WiFi setup via captive portal
- **EEPROM Persistence**: All settings saved across reboots
- **REST API**: Complete control via HTTP endpoints
- **Responsive Web UI**: Multilingual interface (6 languages)
- **mDNS Support**: Access via `railhub8266.local`

## Target Audience

This documentation is intended for:

- **Software Developers**: Understanding codebase, extending functionality
- **System Integrators**: Integrating RailHub8266 into larger systems
- **Product Owners**: Understanding capabilities and limitations
- **Technical Architects**: Evaluating design decisions

## Assumptions

This documentation is based on:

1. **Source Code Analysis**: Repository structure as of 2024
2. **PlatformIO Configuration**: `platformio.ini` build settings
3. **README.md**: User-facing documentation
4. **Hardware Constraints**: ESP8266 capabilities (80KB RAM, 4MB flash, 7 usable GPIO pins)

Where information was unclear or missing, assumptions are clearly marked in the relevant sections.

## Version History

- **2024-11-16**: Initial arc42 documentation created based on v2.0.0 firmware

---

**Documentation Language**: English  
**Template**: arc42 v8.0  
**Last Updated**: November 16, 2025
