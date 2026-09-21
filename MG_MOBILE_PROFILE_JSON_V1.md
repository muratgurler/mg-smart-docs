# MG Mobile Profile JSON V1

The Android/iOS companion app sends a **prepared electrical profile**, not an image, to the ESP32-P4.

## Canonical object

```json
{
  "productionPr": "PR123456",
  "customerReference": "ASML-REF-98765",
  "revision": "R07",
  "profileId": "ASML-HARTING-6-R07",
  "signalPinCount": 6,
  "includePeA": true,
  "includePeB": true,
  "includeDrainShieldA": false,
  "includeDrainShieldB": false,
  "connectorA": {
    "kind": "HARTING",
    "gender": "MALE",
    "contacts": 10,
    "name": "X1"
  },
  "connectorB": {
    "kind": "HARTING",
    "gender": "FEMALE",
    "contacts": 10,
    "name": "X2"
  },
  "nets": [
    {"A": [0], "B": [0]},
    {"A": [1], "B": [1, 2]},
    {"A": [2], "B": [3]},
    {"A": [3, 4], "B": [4]},
    {"A": [5], "B": [5]}
  ]
}
```

## Test-point numbering

The profile uses the tester's internal 64-point numbering on each side:

- `0` = PE
- `1..62` = signal conductors
- `63` = drain/shield (`dS`)

`signalPinCount` determines which signal indices are enabled. PE and dS are independently enabled for A and B using their boolean fields.

## Connector mapping

For the current two-end document convention:

- customer/document `X1` maps to tester side `A`
- customer/document `X2` maps to tester side `B`

The app must show this mapping to the operator before sending when the source document is ambiguous.

## Net semantics

Each item in `nets` is one electrical connected component. It is **not limited to one-to-one wiring**.

Examples:

```json
{"A":[1],"B":[1]}
```
One-to-one.

```json
{"A":[1],"B":[1,2]}
```
One A point connected to multiple B points.

```json
{"A":[3,4],"B":[4]}
```
Multiple A points in the same electrical net, connected to B4.

A physical point may belong to only one net. Reusing the same point in multiple net objects is rejected.

Unused/NC/SPARE conductors are represented by simply omitting that point from `nets`.

## Identity fields

For workplace production profiles, the app should supply all four fields:

- `productionPr`
- `customerReference`
- `revision`
- `profileId`

For a generic/manual non-workplace profile the P4 can fill explicit mobile-import placeholders when identity fields are missing, but the app should still provide a meaningful `profileId` whenever possible.

## Transfer constraints

- File type: JSON object only.
- Maximum file size: 256 KiB.
- One prepared profile per temporary transfer session.
- P4 verifies size and CRC32 before parsing.
- P4 performs its own electrical topology validation before allowing the profile into the test workflow.

The companion app may preserve source-image/PDF metadata on the phone, but it should not embed large source images inside this JSON.

## Companion-app session handshake

After joining the temporary MG Wi-Fi AP, the app calls:

```text
GET http://192.168.4.1/api/profile/session
```

The response provides `token`, `format`, and `maxBytes`. The app then uploads the prepared JSON using `/api/profile/upload` or the resumable `/api/profile/chunk` endpoint with that token.
