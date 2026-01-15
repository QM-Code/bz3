# TODO

## Email-based password resets

- Add SMTP configuration to `config.json` (host, port, user, password, from address).
- Send reset tokens via email instead of displaying the link on the reset request page.
- Add rate limiting and a generic response to avoid email enumeration.

## OAuth logins (Google/Apple/Meta)

- Register OAuth applications and obtain client IDs/secrets.
- Add provider configuration to `config.json`.
- Implement OAuth authorization flow, callback handling, and account linking.
- Store provider identities in a new `user_identities` table.

## Streams

- Look into having a link for a Twitch stream of a game in progress.

## Account state management

- Add UI and workflows for managing locked and deleted accounts (unlock, restore, and audit visibility).

## Tags

- Implement a tagging system managed under `/tags` (tags + descriptions), and allow users to tag worlds (#team, #ctf, #ffa, etc).

## Ratings

- Implement a rating system for worlds and users.

## API namespace

- Consolidate additional JSON endpoints under `/api` (users, worlds, tags, ratings, uploads metadata, etc).
