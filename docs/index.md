---
# https://vitepress.dev/reference/default-theme-home-page
layout: home

hero:
  name: JZ-HDL
  tagline: Hardware Correctness by Construction
  actions:
    - theme: brand
      text: Getting Started
      link: /getting-started/overview
    - theme: alt
      text: Examples
      link: /examples/counter

features:
  - title: Width Safety
    details: All literals must be explicitly sized. Implicit truncation and extension are compile errors. Silent overflow and bit-loss are impossible to express.
  - title: Exclusive Assignment
    details: Every assignable signal is written at most once per execution path. Multiple-driver bugs, priority conflicts, and race conditions are caught at compile time.
  - title: Clock Domain Enforcement
    details: Registers are bound to a single clock domain. Cross-domain reads require explicit CDC declarations. Metastability bugs cannot reach synthesis.
  - title: Compile-Time Guarantees
    details: If it compiles, the hardware is valid. No x propagation to observable sinks, no floating nets, no combinational loops, no unsized literals.
---
