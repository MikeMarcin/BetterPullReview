# External Adapter Instructions

Adapters implement narrow application-host ports.

- Provider/library records do not enter the core or Qt adapter.
- Credentials remain within their designated host capability.
- Normalize untrusted input under explicit size and failure limits.
- Use recorded, mocked, or local fixtures by default.
- Do not add remote mutation tests without a user-designated safe target.
