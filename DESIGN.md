# M2 Technical & Design Understanding

This file is an assessed technical-understanding artifact, not ordinary project documentation.
Answer all four questions using your own submitted implementation. Concise answers are acceptable when they are technically correct and specific.

Generic descriptions of C++ concepts or restatements of the assignment that do not identify and explain corresponding parts of your code will receive limited credit.

## 1. Polymorphism and dynamic dispatch - 1.5 points

One place runtime polymorphism happens in my code is in `ProcessingCore::search()`. The base class is `RetrievalStrategy` and the default derived class is `RetrievalEngine`. `ProcessingCore` stores the retrieval strategy as a `std::unique_ptr<RetrievalStrategy>` and calls `impl_->retrieval->search(...)`.

Since `search()` is virtual, the version that runs depends on the actual object stored in the pointer. For example, in my student tests I pass in a `MarkerRetrievalStrategy`, so calling `ProcessingCore::search()` uses its `search()` function instead of the one from `RetrievalEngine`. If `search()` was not virtual, this runtime dispatch would not work and `ProcessingCore` would not be able to use different retrieval strategies this way.

## 2. Ownership and lifetime - 1.5 points
In the default constructor, the strategies are created using `std::make_unique`. For example, the retrieval strategy is created with `std::make_unique<RetrievalEngine>()`. The pointers are moved into the configurable constructor and then moved into `Impl`, which becomes the owner of the strategies.

Using `std::unique_ptr` means each strategy has one owner and is automatically destroyed when the owning `Impl` is destroyed. This also avoids needing manual `new` and `delete`. `ProcessingCore` is move-only because the `unique_ptr` objects cannot be copied, but their ownership can be moved to another `ProcessingCore`.

The strategy base classes also need virtual destructors because a derived object such as `RetrievalEngine` can be destroyed through a pointer to its base class `RetrievalStrategy`. The virtual destructor makes sure the derived object is destroyed correctly.

## 3. Architecture, extensibility, and M1 compatibility - 1.5 points

The main change I made for M2 was having `ProcessingCore` store the three components through `ChunkingStrategy`, `RetrievalStrategy`, and `ContextStrategy` instead of depending only on the concrete M1 classes.

The default constructor still creates `Chunker`, `RetrievalEngine`, and `ContextBuilder`, so using `ProcessingCore` normally keeps the same M1 behavior. The other constructor lets a caller provide different classes that implement the strategy interfaces. The caller can still use the same `rebuild()`, `search()`, and `build_context()` functions without changing the normal `ProcessingCore` API.

An alternative would be to keep the concrete classes directly in `ProcessingCore` and use conditionals to select different behavior. That would make `ProcessingCore` depend on every possible implementation. Using the strategy interfaces is better for M2 because another implementation can be added without changing the main `ProcessingCore` interface or rewriting the existing M1 classes.

## 4. Testing and defect reasoning - 1.5 points

One important test I wrote is `test_custom_strategy_dispatch()`. It checks that the custom strategies passed into `ProcessingCore` are actually being used.

The test creates `MarkerChunkingStrategy`, `MarkerRetrievalStrategy`, and `MarkerContextStrategy`. Each one returns an easy to recognize value such as `"custom chunk"`, `"custom-result"`, or `"custom-context"`. I then call `rebuild()`, `search()`, and `build_context()` and check that those values are returned.

This would catch a bug where `ProcessingCore` accepts the custom strategies but still uses the original `Chunker`, `RetrievalEngine`, or `ContextBuilder` internally. It also gives additional evidence beyond the public tests because my test defines its own strategies and verifies that all three are reached through runtime dispatch.