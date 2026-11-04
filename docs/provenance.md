# Provenance

Hanji continues the quarter-view runtime that Han wrote for his own Unreal Engine 5.8 game in October 2026, with OpenAI Codex as a coding assistant and without version control. This page lists the evidence that the work existed before this repository and before the programme invitation that prompted publishing it (29 October 2026, 19:30 UTC). For each item it says what kind of evidence it is and what it does and does not show. All times are UTC.

## The 18 files

Copied unchanged into [`prototype/`](../prototype/) on 4 November 2026 from the game module `GyeolLab/Source/GyeolLab`. Creation and last-write times are the filesystem's, read on 4 November 2026; they are local and can be changed, so they are supporting evidence, not proof. The SHA-256 values are of the files as copied; `prototype/** -text` in `.gitattributes` stores them byte for byte, so a checkout reproduces the hashes.

| File | Created | Last written | SHA-256 |
|---|---|---|---|
| `QuarterView/QuarterViewMath.h` | 2026-10-19 03:44 | 2026-10-20 10:44 | `403263335c625f1cd6db21d9a847d12bbd4f5f7abe9a49119a8d863564a4135c` |
| `QuarterView/QuarterViewCamera.h` | 2026-10-19 03:44 | 2026-10-19 14:35 | `e526a8ce511351f594ebaf413d7df46bcd872c0692d6062058078be3a928cebf` |
| `QuarterView/QuarterViewCamera.cpp` | 2026-10-19 03:44 | 2026-10-19 14:46 | `275de34f13b47bdbfaeb727d0ee3a01ad5f4cf92f9cc514a540f8ddedc75d067` |
| `QuarterView/QuarterViewLab.h` | 2026-10-19 03:44 | 2026-10-28 13:36 | `7403da3c6925370084050bbb2fad86b5acfc7947f9e75fbdd9b6b301de373448` |
| `QuarterView/QuarterViewLab.cpp` | 2026-10-19 03:44 | 2026-10-21 03:04 | `709b855f9265e341cfb0e52887e7d67e900a8af87fc20cbb2b345631f1bbce08` |
| `Tests/QuarterViewCameraTests.cpp` | 2026-10-19 03:44 | 2026-10-20 10:44 | `aeb86c8f9d8701a9cca3b499c11ea3089e52631a58df5627e7aa468b52e504d6` |
| `QuarterView/PixelProbe.h` | 2026-10-19 04:36 | 2026-10-30 02:33 | `9bd5826bbe2a582922bb066c06e3301bdec5726719dd79034e7982f44abfee84` |
| `QuarterView/PixelProbe.cpp` | 2026-10-19 04:36 | 2026-10-30 02:40 | `5f126a620d245bd26de10d2805973f186ced3dbe9a6561c4f75883eb8a08b117` |
| `QuarterView/PoseAtlas.h` | 2026-10-19 06:02 | 2026-10-30 02:40 | `fec37be9ef7822c180f68d38c6c9e217c668c7c37ce9bc6f7a44b9b58a5ab4e5` |
| `Tests/PoseAtlasTests.cpp` | 2026-10-19 06:03 | 2026-10-30 02:40 | `15506989333d856bf2e9bfbce379241ccff5de26c6b65949053aa09bd1b76c46` |
| `QuarterView/StrideClock.h` | 2026-10-19 06:44 | 2026-10-20 00:22 | `acced0181a31ec2ed1efad323483ea41d82c2e4e510fbd52099376c5bfa85bea` |
| `QuarterView/NormalProbe.cpp` | 2026-10-19 22:46 | 2026-10-19 23:19 | `699bc2aa84c6438565bcc8dd190b005b499ff51df2012db8aaacb17263961399` |
| `Opening/ContactShadow.h` | 2026-10-20 00:40 | 2026-10-20 00:40 | `5ab93f2e67344c9e2500da7ec23efa296d2504bdad07eb4055c19382233afd05` |
| `Opening/ContactShadow.cpp` | 2026-10-20 00:40 | 2026-10-20 00:40 | `a287c9321d3281de1151b84e51390b2eb6390b53a3936d8e4dc1d50ffd09d39a` |
| `Tests/ContactShadowTests.cpp` | 2026-10-20 00:40 | 2026-10-20 05:13 | `79757ca5e807510c019fb6041f43cb6e3cbdcd3c8d8b770ffabd8462afb4f4e0` |
| `QuarterView/StudyLighting.h` | 2026-10-20 03:35 | 2026-10-20 03:35 | `21307c65fb697688d4805a56c04e717c3876a72161402e7b4d2d2396fa3ee6de` |
| `QuarterView/LocomotionStop.h` | 2026-10-20 16:19 | 2026-10-20 19:25 | `5f526a3ca10f87bf3604bd0ead441fb3789f2a9c5a8defaacb1b4911c069926c` |
| `QuarterView/PlayerOcclusion.cpp` | 2026-10-20 17:34 | 2026-10-20 17:44 | `a9de08e9f7ccf7582ab57a1b334ee641a9401b559c9aea45fa95d043ccbdce24` |

All 18 files were created on 19 and 20 October 2026. Eleven were last written on 19 or 20 October; `QuarterViewLab.cpp` on 21 October, `QuarterViewLab.h` on 28 October, and `PixelProbe.h/.cpp`, `PoseAtlas.h` and `PoseAtlasTests.cpp` on 30 October, the day after the invitation, because the game kept using them. The copies here are the 30 October state; the assistant's session records (below) are the way to see the state before the invitation.

## The assistant's session records

OpenAI Codex keeps a local record of each session with every file edit and command. A request to extract the entries for these 18 files up to the invitation, replay the recorded edits and compare the resulting hashes with the table above was made on 4 November 2026. This section will hold the result: sessions, first and last writes per file, replayed hashes, and recorded builds and automation test runs. Until then, the filesystem times above are the only dated evidence on this page.

## Server-side timestamps

None located yet for this code. A negative result of a limited search, not a statement that none exists.

## What this shows, and what it does not

- **The hashes show** that the files in `prototype/` are the files in the game module as of 4 November 2026. They do not show when those files were written.
- **The filesystem times show** creation on 19 and 20 October and continued edits to 30 October. They are local records.
- **Nothing here yet** is independent of Han's PC.
