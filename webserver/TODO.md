# TODO

## Architecture improvements

- Introduce a routing layer with path templates and method dispatch to replace manual if/else routing.
- Separate handler logic into service-layer/domain functions (validation + business rules).
- Add a lightweight schema migration system to handle database upgrades safely.
- Expand test tooling to cover auth/CSRF and DB integrity (currently only string validation + dev helpers).

## Account state management

- Add UI and workflows for managing locked and deleted accounts (unlock, restore, and audit visibility).

## Email-based password resets

- Add SMTP configuration to `config.json` (host, port, user, password, from address).
- Send reset tokens via email instead of displaying the link on the reset request page.
- Add rate limiting and a generic response to avoid email enumeration.

## Tags

- Implement a tagging system managed under `/tags` (tags + descriptions), and allow users to tag worlds (#team, #ctf, #ffa, etc).

## Ratings

- Implement a rating system for worlds and users.

## API namespace

- Consolidate additional JSON endpoints under `/api` (worlds, tags, ratings, uploads metadata, etc).
- Add API versioning (e.g., `/api/v1`) and document supported versions.


## OAuth logins (Google/Apple/Meta)

- Register OAuth applications and obtain client IDs/secrets.
- Add provider configuration to `config.json`.
- Implement OAuth authorization flow, callback handling, and account linking.
- Store provider identities in a new `user_identities` table.

## Streams

- Look into having a link for a Twitch stream of a game in progress.

## Localization

- Internationalize all scripts using strings/*.json.
