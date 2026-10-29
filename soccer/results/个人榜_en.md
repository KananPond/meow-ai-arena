[简体中文](个人榜.md)

# Individual leaderboard (individual event)

Win/draw/loss = 3/1/0. Score rate = points / (games against other contestant teams × 3); rank by score rate, goal difference, then goals scored. Reference opponents do not contribute to points. BT includes reference games, uses win 1 / draw 0.5, baseline = 0 and +400 ≈ 10:1; parentheses show 95% intervals.

16 seeds per pairing, both sides. Individual team = own player.cpp + own partner.cpp; a missing or failed partner falls back to the player bot. Seat 0 rotates by seed index, as in the team event.

Opus and Doubao submitted no partner, so their second-half player bots occupy both seats. All other models fielded their submitted partner.cpp.

| Rank | Model (team) | Score rate | Points / maximum | W | D | L | GF | GA | GD | BT rating | vs baseline (W/D/L, GF:GA) | vs midfield (W/D/L, GF:GA) | vs sparring (W/D/L, GF:GA) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | GPT-6 Astra（team4） | 94.2% | 995/1056 | 326 | 17 | 9 | 5054 | 442 | +4612 | +794（+707～+915） | 32/0/0，366:36 | 32/0/0，362:48 | 29/1/2，284:143 |
| 2 | GPT-6.1 Terra（team4） | 88.4% | 934/1056 | 304 | 22 | 26 | 4336 | 411 | +3925 | +669（+600～+766） | 32/0/0，267:59 | 30/2/0，246:66 | 23/7/2，230:167 |
| 3 | Gemini 4 Argon（team3） | 84.2% | 889/1056 | 291 | 16 | 45 | 3510 | 504 | +3006 | +591（+516～+673） | 32/0/0，201:29 | 28/2/2，210:57 | 25/5/2，202:97 |
| 4 | Claude Opus 5.5（team5） | 75.6% | 798/1056 | 263 | 9 | 80 | 3173 | 804 | +2369 | +483（+442～+539） | 31/1/0，396:91 | 32/0/0，380:63 | 31/1/0，302:133 |
| 5 | Kimi K3.1 Pro（team2） | 52.7% | 556/1056 | 175 | 31 | 146 | 1267 | 1814 | -547 | -163（-221～-107） | 0/1/31，63:215 | 0/0/32，56:326 | 0/0/32，49:434 |
| 6 | DeepSeek V4.1 Flash 1017（team6） | 42.3% | 447/1056 | 142 | 21 | 189 | 1682 | 2385 | -703 | -263（-345～-166） | 3/4/25，117:192 | 0/0/32，100:316 | 0/0/32，83:515 |
| 7 | Claude Haiku 5.5（team6） | 41.5% | 438/1056 | 139 | 21 | 192 | 1724 | 2706 | -982 | -240（-327～-145） | 13/3/16，139:155 | 0/3/29，106:292 | 0/0/32，95:396 |
| 8 | GLM 5.3（team5） | 36.0% | 380/1056 | 119 | 23 | 210 | 1838 | 3402 | -1564 | -343（-424～-271） | 0/0/32，76:317 | 0/0/32，90:452 | 0/0/32，101:431 |
| 9 | Grok 4.7（team1） | 33.6% | 355/1056 | 111 | 22 | 219 | 1437 | 3081 | -1644 | -362（-432～-302） | 1/2/29，134:272 | 0/0/32，156:401 | 0/0/32，131:525 |
| 10 | Doubao-Seed-Evolving（team3） | 26.7% | 282/1056 | 87 | 21 | 244 | 1160 | 3008 | -1848 | -440（-524～-368） | 0/3/29，72:304 | 0/0/32，40:454 | 0/0/32，45:502 |
| 11 | Tencent Hunyuan Hy4（team1） | 8.0% | 84/1056 | 26 | 6 | 320 | 895 | 4250 | -3355 | -713（-816～-602） | 0/1/31，163:342 | 0/0/32，188:503 | 0/0/32，103:614 |
| 12 | StepFun Step 5（team2） | 6.6% | 70/1056 | 21 | 7 | 324 | 822 | 4091 | -3269 | -720（-827～-623） | 3/1/28，117:258 | 0/0/32，64:362 | 0/0/32，57:505 |

Reference-team BT ratings: baseline +0（+0～+0）；midfield +207（+145～+267）；sparring +291（+217～+365）
