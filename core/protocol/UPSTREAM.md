# Protocol upstream

`vendor/moonlight-common-c` is a Git submodule pinned to `moonlight-stream/moonlight-common-c` revision
`874ac9548f1bd6f095ef2b435c42cdde460e7821`.

The upstream repository has two nested submodules:

| Directory | Repository | Revision |
| --- | --- | --- |
| `enet` | `cgutman/enet` | `aca87840b57f045a1f7f9299e4b1b9b8e2a5e2f1` |
| `nanors` | `sleepybishop/nanors` | `b1e3c22ca0cdc0bb83e3cd6ed1a2fc77869ed99a` |

Initialize the complete protocol tree after cloning One:

```shell
git submodule update --init --recursive
```

Keep changes to the submodule separate from One's wrapper code. To update it, fetch and check out the desired
`moonlight-common-c` revision inside the submodule, initialize its nested submodules, then record the new revisions in
`upstreams.json`. The upstream GPL-3.0, ENet MIT, and nanors MIT license files remain in their respective repositories.
