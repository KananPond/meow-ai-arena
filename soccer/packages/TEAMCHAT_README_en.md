[简体中文原件](TEAMCHAT_README.md)

# Team chat: contestant instructions

Reference translation of the original distributed document. The Chinese original, including its conditional reminder wording, is retained unchanged. The actual integration limitation is documented in [conditions](../CONDITIONS_en.md).

You and your teammate develop at the same time and each control one player. Use the chat tool to communicate. Run `tc.py` (or `tc.bat`) from your own folder; no identity argument is needed.

| Command | Purpose |
|---|---|
| `python tc.py send "message"` | Send a message to your teammate |
| `python tc.py send -f msg.txt` | Send a long message from a file, including multiple lines or code |
| `python tc.py send -` | Read a message from standard input |
| `python tc.py inbox` | Read unread messages and mark them read |
| `python tc.py wait 60` | Wait for a teammate message; return as soon as one arrives. Default 120 seconds, maximum 600 |
| `python tc.py log` | Show the last 20 messages; use `--last 50` or `--all` for more |
| `python tc.py status` | Show unread count and the teammate's last message time |

`tc.bat` is equivalent to `python tc.py`; Windows also accepts `tc send "..."`.

- You may read your teammate's code directory, but must not modify it. Use `status` to find it. You may write in your own directory.
- After asking for a reply, use `wait`, not a busy polling loop. If no answer arrives, continue working and check `inbox` later.
- Be specific about interfaces, parameters, coordination and code locations. Put long messages in a file and send its location in chat.
- Chat is recorded in full and cannot be deleted or edited. Do not edit any files in the `chat` directory.
- **If** compilation, match or replay tool output ends with an unread-message reminder, your teammate has sent a message; run `inbox` promptly.

## Optional reminders in your own scripts

```python
import tc      # tc.py is in the same folder
...            # normal output
tc.notify()    # append a line only if there are unread messages
```

Alternatively wrap a command: `python with_notice.py g++ -O2 -o me.exe me.cpp`.

## Common problems

- Output defaults to UTF-8. To send Chinese text through a pipeline, use `send -f file` with UTF-8 or GBK input.
- If a message starts with `-`, use `send -- "-message"`.

These are historical instructions. This archive does not include the live chat service or identity configuration; see the [package inventory and reconstruction guide](restore_en.md).
