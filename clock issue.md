

      "derived": {
        "PRIMITIVE": {
          "description": "Primitive name to use",
          "if": "REF_CLK.source == 'pad'",
          "then": "SB_PLL40_PAD",
          "else": "SB_PLL40_CORE"
        }
      },