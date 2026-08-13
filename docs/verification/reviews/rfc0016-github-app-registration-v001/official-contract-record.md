# GitHub App registration official-contract retrieval record

Retrieved: 2026-08-13 (America/Los_Angeles)

Method: `curl --fail --silent --show-error --location`, with the resulting
HTTP response body hashed byte-for-byte by `shasum -a 256`. No credentials,
cookies, response bodies, or third-party documentation copies are retained.

| Official GitHub documentation | HTTP | Retrieved HTML SHA-256 | Bytes | Decision fact supported |
|---|---:|---|---:|---|
| [Generating a user access token for a GitHub App](https://docs.github.com/en/apps/creating-github-apps/authenticating-with-a-github-app/generating-a-user-access-token-for-a-github-app) | 200 | `9d646ebf3be5be5f478c921103af6ccd7440854b7b3ea286869ecc56b4ecb9e3` | 342313 | GitHub Apps support device authorization through the public client identifier when device flow is enabled; the device and token exchanges are distinct. |
| [Refreshing user access tokens](https://docs.github.com/en/apps/creating-github-apps/authenticating-with-a-github-app/refreshing-user-access-tokens) | 200 | `5f846f6f74411886ef41b9e8b54c67391f6a23bf449a81bdebb72fa6224c44d8` | 201109 | Expiring GitHub App user tokens have a refresh lifecycle, and a token originating in device flow can be refreshed without a client secret. |
| [Choosing permissions for a GitHub App](https://docs.github.com/en/enterprise-cloud@latest/apps/creating-github-apps/registering-a-github-app/choosing-permissions-for-a-github-app) | 200 | `87927916b0cb745fb5a562bf48e7a7401c1ffaff6c10155c5bcbf70f58073d41` | 209094 | GitHub App authority is permission- and installation-scoped; exact REST and GraphQL operations must be tested against the selected permissions. |
| [Scopes for OAuth Apps](https://docs.github.com/en/apps/oauth-apps/building-oauth-apps/scopes-for-oauth-apps?apiVersion=2022-11-28) | 200 | `3a9cb52ee2fa959ae32fffa46935212b0b615f71c2efe336ffd2f2db96d82308` | 224975 | The OAuth `repo` scope grants broad public/private repository authority beyond BetterPullReview's currently shipped read and review-comment surface. |

## Local operation inventory joined to the official model

The repository's closed operation catalogs require Metadata read, Contents
read, Checks read, and Pull requests read/write. They contain no shipped
Issues, Actions, Workflows, Commit statuses, Administration, organization,
enterprise, or account operation. The exact review-thread GraphQL document and
all REST routes still require positive and negative real app-token probes;
documentation alone is not accepted as permission sufficiency.

The content hashes above bind the documentation bytes actually consulted. A
later revalidation that retrieves different bytes requires a new record and a
new attended decision before the changed material is relied upon.
