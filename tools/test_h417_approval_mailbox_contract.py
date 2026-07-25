from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(*parts: str) -> str:
    return ROOT.joinpath(*parts).read_text(encoding="utf-8")


def test_mailbox_address_and_payload_bounds_are_product_fixed():
    header = read("firmware", "common", "aik_approval_mailbox.h")

    assert "#define AIK_APPROVAL_MAILBOX_ADDRESS       0x20178800u" in header
    assert "#define AIK_APPROVAL_MAILBOX_REGION_BYTES  2048u" in header
    assert "#define AIK_APPROVAL_MAILBOX_ALIGNMENT     64u" in header
    assert "#define AIK_APPROVAL_MAILBOX_VERSION       1u" in header
    assert "#define AIK_APPROVAL_TOOL_MAX              16u" in header
    assert "#define AIK_APPROVAL_SUMMARY_MAX           120u" in header
    assert "#define AIK_CLAUDE_STATE_OFF               0u" in header
    assert "#define AIK_CLAUDE_STATE_RUNNING           1u" in header
    assert "#define AIK_CLAUDE_STATE_DONE              2u" in header
    assert "uint8_t claude_state;" in header
    assert "uint8_t reserved[2];" in header
    assert "volatile uint32_t sequence;" in header
    assert "payload_crc16" in header
    assert "payload->claude_state > AIK_CLAUDE_STATE_DONE" in header
    assert "fence iorw, iorw" in header
    assert "aik_approval_mailbox_retained_sequence" in header


def test_v3_initializes_mailbox_before_the_existing_v5_wake_call():
    board = read(
        "firmware", "h417", "v3f", "applications", "board_init.c"
    )
    main = read("firmware", "h417", "v3f", "applications", "main.c")

    assert "v3f_approval_mailbox_init();" in board
    assert main.index("v3f_board_init();") < main.index("v3f_board_start_v5f();")


def test_v3_preserves_claude_state_across_approval_and_off_clears_it():
    mailbox = read(
        "firmware", "h417", "v3f", "applications", "approval_mailbox.c"
    )
    mailbox_header = read(
        "firmware", "h417", "v3f", "applications", "approval_mailbox.h"
    )

    assert "v3f_approval_mailbox_set_claude_state" in mailbox_header
    assert "uint8_t claude_state = s_payload.claude_state;" in mailbox
    assert "claude_state = s_payload.claude_state;" in mailbox
    assert "s_payload.claude_state = claude_state;" in mailbox
    assert "Process exit invalidates any approval that belonged to it." in mailbox


def test_pc_link_exposes_strict_approval_show_and_clear_commands():
    pc_link = read(
        "firmware", "h417", "v3f", "applications", "pc_link.c"
    )

    assert '"APPROVAL"' in pc_link
    assert '"SHOW"' in pc_link
    assert '"CLEAR"' in pc_link
    assert "pc_parse_fixed_hex(&tag_token, 8u" in pc_link
    assert "pc_parse_fixed_hex(&risk_token, 1u" in pc_link
    assert "pc_decode_ascii_hex" in pc_link
    assert "v3f_approval_mailbox_show" in pc_link
    assert "v3f_approval_mailbox_clear" in pc_link
    assert '"CLAUDE"' in pc_link
    assert '"STATE"' in pc_link
    assert '"OFF"' in pc_link
    assert '"RUNNING"' in pc_link
    assert '"DONE"' in pc_link
    assert "pc_tokens_finished(args)" in pc_link
    assert "v3f_approval_mailbox_set_claude_state" in pc_link
    assert "(line[6] == '\\0') || (line[6] == ' ')" in pc_link


def test_v5_product_ui_polls_only_published_snapshots_at_20ms():
    makefile = read("firmware", "h417", "v5f_rtthread", "Makefile")
    main = read(
        "firmware", "h417", "v5f_rtthread", "applications", "main.c"
    )
    ui = read(
        "firmware",
        "h417",
        "v5f_rtthread",
        "applications",
        "v5f_approval_ui.c",
    )

    assert 'INC += -I"../../common"' in makefile
    assert "applications/v5f_approval_ui.c" in makefile
    assert "#define APP_MAIN_LOOP_DELAY_MS 20" in main
    assert "v5f_approval_ui_poll();" in main
    assert "aik_approval_mailbox_read" in ui
    assert "sequence == s_last_sequence" in ui
    assert "payload.claude_state == AIK_CLAUDE_STATE_RUNNING" in ui
    assert "payload.claude_state == AIK_CLAUDE_STATE_DONE" in ui
    assert "v5f_claude_ui_draw(payload.claude_state);" in ui
    assert "v5f_competition_ui_draw();" in ui
    assert ui.index("ui_draw_approval(&payload);") < ui.index("v5f_claude_ui_draw")
    assert "s_ui_font8x8[95][8]" in ui
    for label in (
        "LOW",
        "MEDIUM",
        "HIGH",
        "CRITICAL",
        "DESTRUCTIVE",
        "Yes",
        "No",
        "Fn+stick up/down: select | Fn+knob press: confirm",
    ):
        assert f'"{label}"' in ui
