static const ASM_MAPPING _asxxxx_gb_mapping[] = {
    /* We want to prepend the _ */
    { "area", ".area _%s" },
    { "areacode", ".area _%s" },
    { "areadata", ".area _%s" },
    { "areahome", ".area _%s" },
    { "functionlabeldef", "%s:" },
    { "*hl", "(hl)" },
    { "di", "di" },
    { "ldahli", "ld a,(hl+)" },
    { "ldahlsp", "lda hl,%d(sp)" },
    { "ldaspsp", "lda sp,%d(sp)" },
    { "*pair", "(%s)" },
    { "shortjp", "jr" },
    { "enter", "" },
    { "enterx", 
      "lda sp,-%d(sp)" },
    { "enterxl",
                "ld hl,#-%d\n"
                "\tadd\thl,sp\n"
                "\tld\tsp,hl"
    },
    { "leave", ""
    },
    { "leavex", "lda sp,%d(sp)"
    },
    { "leavexl",
                "ld hl,#%d\n"
                "\tadd\thl,sp\n"
                "\tld\tsp,hl"
    },
    { "pusha", 
      "push af\n"
      "\tpush bc\n"
      "\tpush de\n"
      "\tpush hl"
    },
    { "adjustsp", "lda sp,-%d(sp)" },
    { "fileprelude", "" },
    { "profileenter",
                "ld a,#3\n"
                "\trst\t0x08"
    },
    { "profileexit",
                "ld a,#4\n"
                "\trst\t0x08"
    },
    { NULL, NULL }
};

static const ASM_MAPPING _asxxxx_z80_mapping[] = {
    /* We want to prepend the _ */
    { "area", ".area _%s" },
    { "areacode", ".area _%s" },
    { "areadata", ".area _%s" },
    { "areahome", ".area _%s" },
    { "*ixx", "%d(ix)" },
    { "*iyx", "%d(iy)" },
    { "*hl", "(hl)" },
    { "di", "di" },
    { "ldahli", 
		"ld a,(hl)\n"
		"\tinc\thl" },
    { "ldahlsp", 
		"ld hl,#%d\n"
		"\tadd\thl,sp" },
    { "ldaspsp", 
		"ld hl,#%d\n"
		"\tadd\thl,sp\n"
		"\tld\tsp,hl" },
    { "*pair", "(%s)" },
    { "shortjp", "jp" },
    { "enter", 
		"push\tix\n"
		"\tld\tix,#0\n"
		"\tadd\tix,sp" },
    { "enterx", 
		"push\tix\n"
		"\tld\tix,#0\n"
		"\tadd\tix,sp\n"
		"\tld\thl,#-%d\n"
		"\tadd\thl,sp\n"
		"\tld\tsp,hl" 
        },
    { "leave", 
		"pop\tix"
    },
    { "leavex", 
		"ld sp,ix\n"
		"\tpop\tix"
    },
    { "pusha", 
      		"push af\n"
      		"\tpush\tbc\n"
      		"\tpush\tde\n"
      		"\tpush\thl"
    },
    { "adjustsp", "lda sp,-%d(sp)" },
    { "profileenter",
                "ld a,#3\n"
                "\trst\t0x08"
    },
    { "profileexit",
                "ld a,#4\n"
                "\trst\t0x08"
    },
    { NULL, NULL }
};

static const ASM_MAPPING _rgbds_mapping[] = {
    { "global", "GLOBAL %s" },
    { "extern", "GLOBAL %s" },
    { "slabeldef", "%s:" },
    { "labeldef", "%s:" },
    { "tlabeldef", ".l%05d:" },
    { "tlabel", ".l%05d" },
    { "fileprelude", 
      "; Generated using the rgbds tokens.\n"
      "\t; We have to define these here as sdcc dosnt make them global by default\n"
      "\tGLOBAL __mulschar\n"
      "\tGLOBAL __muluchar\n"
      "\tGLOBAL __mulint\n"
      "\tGLOBAL __divschar\n"
      "\tGLOBAL __divuchar\n"
      "\tGLOBAL __divsint\n"
      "\tGLOBAL __divuint\n"
      "\tGLOBAL __modschar\n"
      "\tGLOBAL __moduchar\n"
      "\tGLOBAL __modsint\n"
      "\tGLOBAL __moduint\n"
      "\tGLOBAL __mullong\n"  
      "\tGLOBAL __modslong\n"  
      "\tGLOBAL __divslong\n"  
      "\tGLOBAL banked_call\n"
      "\tGLOBAL banked_ret\n"
    },
    { "functionheader", 
      "; ---------------------------------\n"
      "; Function %s\n"
      "; ---------------------------------"
    },
    { "functionlabeldef", "%s:" },
    { "zero", "$00" },
    { "one", "$01" },
    { "area", "SECTION \"%s\",CODE" },
    { "areadata", "SECTION \"%F_%s\",BSS" },
    { "areacode", "SECTION \"%F_CODE\",%s" }, 
    { "areahome", "SECTION \"%F_HOME\",HOME" },
    { "ascii", "DB \"%s\"" },
    { "ds", "DS %d" },
    { "db", "DB" },
    { "dbs", "DB %s" },
    { "dw", "DW" },
    { "dws", "DW %s" },
    { "immed", "" },
    { "constbyte", "$%02X" },
    { "constword", "$%04X" },
    { "immedword", "$%04X" },
    { "immedbyte", "$%02X" },
    { "hashedstr", "%s" },
    { "lsbimmeds", "%s & $FF" },
    { "msbimmeds", "%s >> 8" },
    { "bankimmeds", "BANK(%s)" },
    { "module", "; MODULE %s" },
    { NULL, NULL }
};

static const ASM_MAPPING _rgbds_gb_mapping[] = {
    { "pusha", 
      "push af\n"
      "\tpush bc\n"
      "\tpush de\n"
      "\tpush hl"
    },
    { "di", "di" },
    { "adjustsp", "add sp,-%d" },
    { "enter", "" },
    { "enterx", "add sp,-%d"
    },
    { "leave", ""
    },
    { "leavex", "add sp,%d"
    },
    { "ldahli", "ld a,[hl+]" },
    { "*hl", "[hl]" },
    { "ldahlsp", "ld hl,[sp+%d]" },
    { "ldaspsp", "add sp,%d" },
    { "*pair", "[%s]" },
    { "shortjp", "jr" },
    { NULL, NULL }
};

static const ASM_MAPPING _isas_mapping[] = {
    { "global", "GLOBAL %s" },
    { "extern", "GLOBAL %s" },
    { "slabeldef", "%s:" },
    { "labeldef", "%s:" },
    { "tlabeldef", "?l%05d:" },
    { "tlabel", "?l%05d" },
    { "fileprelude", 
      ";Generated using the isas tokens.\n"
      "\tLPREFIX '?'  ; Treat labels starting with ? as local.\n"
      "\tONCNUM       ; Numbers are hex\n"
      "\tCAPSOFF      ; Case sensitive\n"
      "\tISDMG        ; Gameboy mode\n"
      "_CODE\tGROUP\n"
      "\t; We have to define these here as sdcc doesnt make them global by default\n"
      "\tGLOBAL __mulschar\n"
      "\tGLOBAL __muluchar\n"
      "\tGLOBAL __mulint\n"
      "\tGLOBAL __divschar\n"
      "\tGLOBAL __divuchar\n"
      "\tGLOBAL __divsint\n"
      "\tGLOBAL __divuint\n"
      "\tGLOBAL __modschar\n"
      "\tGLOBAL __moduchar\n"
      "\tGLOBAL __modsint\n"
      "\tGLOBAL __moduint\n"
      "\tGLOBAL banked_call\n"
      "\tGLOBAL banked_ret\n"
    },
    { "functionheader", 
      "; ---------------------------------\n"
      "; Function %s\n"
      "; ---------------------------------"
    },
    { "functionlabeldef", "%s:" },
    { "zero", "$00" },
    { "one", "$01" },
    { "area", "%s\tGROUP" },
    { "areacode", "_CODE\tGROUP" },
    { "areadata", "_DATA\tGROUP" },
    { "areahome", "_CODE\tGROUP" },
    { "ascii", "DB \"%s\"" },
    { "ds", "DS %d" },
    { "db", "DB" },
    { "dbs", "DB %s" },
    { "dw", "DW" },
    { "dws", "DW %s" },
    { "immed", "" },
    { "constbyte", "0x%02X" },
    { "constword", "0x%04X" },
    { "immedword", "0x%04X" },
    { "immedbyte", "0x%02X" },
    { "hashedstr", "%s" },
    { "lsbimmeds", "%s & 0xFF" },
    { "msbimmeds", "%s >> 8" },
    { "bankimmeds", "!%s" },
    { "module", "; MODULE %s" },
    { NULL, NULL }
};

static const ASM_MAPPING _isas_gb_mapping[] = {
    { "pusha", 
      "push af\n"
      "\tpush bc\n"
      "\tpush de\n"
      "\tpush hl"
    },
    { "di", "di" },
    { "adjustsp", "add sp,-%d" },
    { "enter", "" },
    { "enterx", "add sp,-%d"
    },
    { "leave", ""
    },
    { "leavex", "add sp,%d\n" },
    { "ldahli", "ld a,(hli)" },
    { "*hl", "(hl)" },
    { "ldahlsp", "ldhl sp,%d" },
    { "ldaspsp", "add sp,%d" },
    { "*pair", "(%s)" },
    { "shortjp", "jr" },
    { NULL, NULL }
};

static const ASM_MAPPING _z80asm_mapping[] = {
    { "global", "XDEF %s" },
    { "extern", "XREF %s" },
    { "slabeldef", "\n.%s" },
    { "labeldef", "\n.%s" },
    { "tlabeldef", "\n.l%N%05d" },
    { "tlabel", "l%N%05d" },
    { "fileprelude", 
        "; Generated using the z80asm/z88 tokens.\n"
        "\tXREF __muluchar_rrx_s\n"
        "\tXREF __mulschar_rrx_s\n"
        "\tXREF __mulint_rrx_s\n"
        "\tXREF __mullong_rrx_s\n"
        "\tXREF __divuchar_rrx_s\n"
        "\tXREF __divschar_rrx_s\n"
        "\tXREF __divsint_rrx_s\n"
        "\tXREF __divuint_rrx_s\n"
        "\tXREF __divulong_rrx_s\n"
        "\tXREF __divslong_rrx_s\n"
        "\tXREF __rrulong_rrx_s\n"
        "\tXREF __rrslong_rrx_s\n"
        "\tXREF __rlulong_rrx_s\n"
        "\tXREF __rlslong_rrx_s\n"
    },
    { "functionheader", 
      "; ---------------------------------\n"
      "; Function %s\n"
      "; ---------------------------------"
    },
    { "functionlabeldef", ".%s" },
    { "zero", "$00" },
    { "one", "$01" },
    { "ascii", "DEFM \"%s\"" },
    { "ds", "DEFS %d" },
    { "db", "DEFB" },
    { "dbs", "DEFB %s" },
    { "dw", "DEFW" },
    { "dws", "DEFB %s" },
    { "immed", "" },
    { "constbyte", "$%02X" },
    { "constword", "$%04X" },
    { "immedword", "$%04X" },
    { "immedbyte", "$%02X" },
    { "hashedstr", "%s" },
    { "lsbimmeds", "%s & $FF" },
    { "msbimmeds", "%s / 256" },

    { "bankimmeds", "BANK(%s)" },
    { "module", "MODULE %s" },
    { "area", "; Area  %s" },
    { "areadata", "; Aread BSS" },
    { "areacode", "; Area CODE" }, 
    { "areahome", "; Area HOME" },
    { NULL, NULL }
};

static const ASM_MAPPING _z80asm_z80_mapping[] = {
    { "*ixx", "(ix%+d)" },
    { "*iyx", "(iy%+d)" },
    { "*hl", "(hl)" },
    { "di", "di" },
    { "ldahli", 
		"ld a,(hl)\n"
		"\tinc\thl" },
    { "ldahlsp", 
		"ld hl,%d\n"
		"\tadd\thl,sp" },
    { "ldaspsp", 
		"ld hl,%d\n"
		"\tadd\thl,sp\n"
		"\tld\tsp,hl" },
    { "*pair", "(%s)" },
    { "shortjp", "jp" },
    { "enter", 
		"push\tix\n"
		"\tld\tix,0\n"
		"\tadd\tix,sp" },
    { "enterx", 
		"push\tix\n"
		"\tld\tix,0\n"
		"\tadd\tix,sp\n"
		"\tld\thl,-%d\n"
		"\tadd\thl,sp\n"
		"\tld\tsp,hl" 
        },
    { "leave", 
		"pop\tix"
    },
    { "leavex", 
		"ld sp,ix\n"
		"\tpop\tix"
    },
    { "pusha", 
      		"push af\n"
      		"\tpush\tbc\n"
      		"\tpush\tde\n"
      		"\tpush\thl"
    },
    { "adjustsp", "lda sp,(sp%+d)" },
    { "profileenter",
                "ld a,3\n"
                "\trst\t$08"
    },
    { "profileexit",
                "ld a,4\n"
                "\trst\t$08"
    },
    { NULL, NULL }
};

static const ASM_MAPPINGS _isas = {
    NULL,
    _isas_mapping
};

const ASM_MAPPINGS _isas_gb = {
    &_isas,
    _isas_gb_mapping
};

static const ASM_MAPPINGS _rgbds = {
    NULL,
    _rgbds_mapping
};

const ASM_MAPPINGS _rgbds_gb = {
    &_rgbds,
    _rgbds_gb_mapping
};

const ASM_MAPPINGS _asxxxx_gb = {
    &asm_asxxxx_mapping,
    _asxxxx_gb_mapping
};

const ASM_MAPPINGS _asxxxx_z80 = {
    &asm_asxxxx_mapping,
    _asxxxx_z80_mapping
};

static const ASM_MAPPINGS _z80asm = {
    NULL,
    _z80asm_mapping
};

const ASM_MAPPINGS _z80asm_z80 = {
    &_z80asm,
    _z80asm_z80_mapping
};
