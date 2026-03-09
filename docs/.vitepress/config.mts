import { defineConfig } from 'vitepress';
import llmstxt from 'vitepress-plugin-llms'
import jzLang from '../shiki/jz-hdl.tmLanguage.json';
import jzTheme from '../shiki/jz-hdl.theme.json';

// https://vitepress.dev/reference/site-config
export default defineConfig({
  base: '/jz-hdl/',
  vite: {
    plugins: [llmstxt()]
  },
  title: "JZ-HDL Reference",
  description: "JZ-HDL is a hardware description language designed for clear, analyzable, and synthesis‑friendly RTL.",
  themeConfig: {
    // https://vitepress.dev/reference/default-theme-config
    nav: [
      { text: 'Home', link: '/' },
      { text: 'Getting Started', link: '/getting-started/overview' },
      { text: 'Examples', link: '/examples/counter' },
      {
        text: 'Specifications',
        items: [
          { text: 'JZ-HDL Specification', link: '/pdf/jz-hdl-specification.pdf', target: '_blank' },
          { text: 'Simulation Specification', link: '/pdf/simulation-specification.pdf', target: '_blank' },
          { text: 'Testbench Specification', link: '/pdf/testbench-specification.pdf', target: '_blank' },
        ]
      },
    ],

    sidebar: [
      {
        text: 'Quick Start',
        items: [
          { text: 'Overview', link: '/getting-started/overview' },
          { text: 'Contract', link: '/getting-started/contract' },
          { text: 'Installation', link: '/getting-started/installation' },
          { text: 'CLI Usage', link: '/getting-started/cli-usage' },
        ]
      }, {
        text: 'Reference Manual',
        items: [
          {
            text: 'Language Reference',
            link: '/reference-manual/formal-reference/core-concepts',
            collapsed: true,
            items: [
              { text: 'Core Concepts', link: '/reference-manual/formal-reference/core-concepts' },
              { text: 'Type System', link: '/reference-manual/formal-reference/type-system' },
              { text: 'Expressions and Operators', link: '/reference-manual/formal-reference/expressions-and-operators' },
              { text: 'Module Structure', link: '/reference-manual/formal-reference/module-structure' },
              { text: 'Statements', link: '/reference-manual/formal-reference/statements' },
              { text: 'Projects', link: '/reference-manual/formal-reference/projects' },
              { text: 'Memory', link: '/reference-manual/formal-reference/memory' },
              { text: 'Global Literals', link: '/reference-manual/formal-reference/global-block' },
              { text: 'Templates', link: '/reference-manual/formal-reference/templates' },
              { text: 'Compile Time Check', link: '/reference-manual/formal-reference/compile-time-check' },
            ]
          },
          { text: 'Net Drivers', link: '/reference-manual/net-drivers' },
          { text: 'Types and Widths', link: '/reference-manual/type-width-system' },
          { text: 'Projects', link: '/reference-manual/projects' },
          { text: 'Modules', link: '/reference-manual/modules' },
          { text: 'Memory', link: '/reference-manual/memory' },
          { text: 'Clock Domains', link: '/reference-manual/clock-domains' },
          { text: 'Tristate Default', link: '/reference-manual/tristate-default' },
          { text: 'Errors and Diagnostics', link: '/reference-manual/errors-diagnostics-warnings' },
          { text: 'Migration', link: '/reference-manual/migration' },
          { text: 'Language Examples', link: '/reference-manual/language-examples' },
        ]
      }, {
        text: 'Verification',
        items: [
          { text: 'Testbench', link: '/reference-manual/testbench' },
          { text: 'Simulation', link: '/reference-manual/simulation' },
        ]
      }, {
        text: 'Specifications (PDF)',
        items: [
          { text: 'JZ-HDL Specification', link: '/pdf/jz-hdl-specification.pdf', target: '_blank' },
          { text: 'Simulation Specification', link: '/pdf/simulation-specification.pdf', target: '_blank' },
          { text: 'Testbench Specification', link: '/pdf/testbench-specification.pdf', target: '_blank' },
        ]
      }, {
        text: 'Examples',
        items: [
          { text: 'Counter', link: '/examples/counter' },
          { text: 'Latch', link: '/examples/latch' },
          { text: 'PLL', link: '/examples/pll' },
          { text: 'DVI', link: '/examples/dvi' },
          { text: 'DVI with Audio', link: '/examples/dvi-audio' },
          { text: 'Ascon-128 Crypto', link: '/examples/ascon' },
          { text: 'CPU Examples', link: '/examples/cpu' },
          { text: 'SOC', link: '/examples/soc' },
        ]
      },
    ],

    socialLinks: []
  },

  markdown: {
    // theme: jzTheme,
    languages: [
      // Force the object to match Shiki's expected LanguageRegistration
      {
        ...jzLang,
        name: 'jz' // Ensure the ID used in ```jz matches the 'name' field
      } as any
    ]
  }
})
