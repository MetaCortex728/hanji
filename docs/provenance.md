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

OpenAI Codex keeps a local record of each session with every file edit and command. On 4 November 2026 the assistant read the October segments of the session in which this code was written (`01a13efd-4a4b-7283-a42c-728222aa7224`), counted only entries before the invitation (29 October 2026, 19:30 UTC), replayed every recorded edit to the 18 files and hashed the result; on 5 November it re-read the files' times and hashes. [`provenance/session-records.json`](provenance/session-records.json) lists all 178 recorded edits and 19 recorded builds and test runs, with session line numbers and call ids. It holds no source code, conversation text or local paths.

| File | First recorded write | Last recorded write before the invitation | Replay up to the invitation vs the file here |
|---|---|---|---|
| `QuarterView/QuarterViewMath.h` | 2026-10-19 03:44:20 | 2026-10-20 10:44:11 | same SHA-256 |
| `QuarterView/QuarterViewCamera.h` | 2026-10-19 03:44:20 | 2026-10-19 14:35:30 | same SHA-256 |
| `QuarterView/QuarterViewCamera.cpp` | 2026-10-19 03:44:20 | 2026-10-19 14:46:27 | same SHA-256 |
| `QuarterView/QuarterViewLab.h` | 2026-10-19 03:44:20 | 2026-10-28 13:36:36 | differs |
| `QuarterView/QuarterViewLab.cpp` | 2026-10-19 03:44:20 | 2026-10-21 03:04:22 | not reconstructable (an edit's content is not fully recorded) |
| `QuarterView/PixelProbe.h` | 2026-10-19 04:36:31 | 2026-10-21 08:44:19 | differs; the file was edited again on 30 October |
| `QuarterView/PixelProbe.cpp` | 2026-10-19 04:36:31 | 2026-10-21 08:44:19 | differs; the file was edited again on 30 October |
| `QuarterView/PoseAtlas.h` | 2026-10-19 06:02:22 | 2026-10-21 08:46:38 | not reconstructable; edited again on 30 October |
| `QuarterView/StrideClock.h` | 2026-10-19 06:44:23 | 2026-10-20 00:22:25 | same SHA-256 |
| `QuarterView/NormalProbe.cpp` | 2026-10-19 22:46:08 | 2026-10-19 23:19:36 | same SHA-256 |
| `QuarterView/StudyLighting.h` | 2026-10-20 03:35:57 | 2026-10-20 03:35:57 | same SHA-256 |
| `QuarterView/LocomotionStop.h` | 2026-10-20 16:19:38 | 2026-10-20 19:25:40 | same SHA-256 |
| `QuarterView/PlayerOcclusion.cpp` | 2026-10-20 17:34:41 | 2026-10-20 17:44:31 | same SHA-256 |
| `Opening/ContactShadow.h` | 2026-10-20 00:40:37 | 2026-10-20 00:40:37 | same SHA-256 |
| `Opening/ContactShadow.cpp` | 2026-10-20 00:40:37 | 2026-10-20 00:40:37 | same SHA-256 |
| `Tests/QuarterViewCameraTests.cpp` | 2026-10-19 03:44:20 | 2026-10-20 10:44:11 | same SHA-256 |
| `Tests/PoseAtlasTests.cpp` | 2026-10-19 06:03:42 | 2026-10-21 08:47:10 | not reconstructable; edited again on 30 October |
| `Tests/ContactShadowTests.cpp` | 2026-10-20 00:40:37 | 2026-10-20 05:13:20 | same SHA-256 |

For 12 of the 18 files, replaying the recorded edits reproduces the file in this repository byte for byte. Of the other six, four were changed again on 30 October, after the invitation, which a replay up to the invitation cannot reproduce (for two of them an earlier edit's content is also not fully recorded); for `QuarterViewLab.cpp` an edit's content is not fully recorded; and for `QuarterViewLab.h` the replay differs although its last recorded edit is before the invitation, for a reason this page does not establish. Each first write in the records comes about one second before the file's creation time on disk (for example `QuarterViewMath.h`: 03:44:20 in the record, 03:44:21 on disk).

The records also hold 19 builds and automation test runs before the invitation, from 19 to 29 October: 13 report success (including four automation passes on 19, 19, 21 and 28 October) and 6 report failure, as the code was being written. A success marker shows that a build or test step ran; it does not show which files that run compiled.

## What this shows, and what it does not

- **The hashes show** that the files in `prototype/` are the files in the game module as of 4 November 2026; a recheck on 5 November found them unchanged. They do not show when those files were written.
- **The session records show** each file being created on 19 or 20 October and edited until 21 October (one file until 28 October), with builds and test runs along the way. For 12 files the replayed edits reproduce the file here byte for byte. The records are local to Han's PC: harder to fake consistently than a file time, but not independent of the PC.
- **The filesystem times** agree with the session records to within about a second. They are local too.
- **No server-side timestamp** was searched for in this round. That is a gap in the evidence, not a statement that none exists.

The full session records and a live comparison can be shown to anyone who wants to check beyond this page.
