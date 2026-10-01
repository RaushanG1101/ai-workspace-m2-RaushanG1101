#include "aiws/chunker.hpp"
#include "aiws/context_builder.hpp"
#include "aiws/processing_core.hpp"
#include "aiws/retrieval_engine.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Student-written M2 tests
//
// Add your own tests to this file. Your tests are part of the submitted work
// and are evaluated under Student-Written Testing & Validation.
//
// Do not modify tests/public_tests.cpp.
//
// Your tests should exercise important M2 behavior beyond the supplied public
// tests. Consider default compatibility, custom strategies, runtime dispatch,
// invalid configuration, ownership/move behavior, and component interactions.

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

class MarkerChunkingStrategy : public aiws::ChunkingStrategy {
public:
    std::vector<aiws::Chunk> chunk(const aiws::Document& document,
                                   std::size_t document_order) const override {
        aiws::Chunk chunk;
        chunk.id = document.id() + ":marker";
        chunk.document_id = document.id();
        chunk.document_order = document_order;
        chunk.sequence = 0;
        chunk.text = "custom chunk";
        chunk.token_count = 2;
        chunk.source_begin = 0;
        chunk.source_end = 0;

        return {chunk};
    }
};

class MarkerRetrievalStrategy : public aiws::RetrievalStrategy {
public:
    std::vector<aiws::SearchResult> search(
        const std::string&,
        int,
        const std::vector<aiws::Chunk>&,
        const aiws::CorpusIndex&) const override {

        aiws::SearchResult result;
        result.chunk_id = "custom-result";
        result.document_id = "custom-document";
        result.chunk_sequence = 0;
        result.text = "custom retrieval";
        result.score = 99.0;
        result.matched_terms = 1;

        return {result};
    }
};

class MarkerContextStrategy : public aiws::ContextStrategy {
public:
    std::vector<aiws::ContextItem> build(
        const std::vector<aiws::SearchResult>&,
        std::size_t) const override {

        aiws::ContextItem item;
        item.chunk_id = "custom-context";
        item.document_id = "custom-document";
        item.chunk_sequence = 0;
        item.text = "custom context";
        item.token_count = 2;
        item.score = 99.0;
        item.truncated = false;

        return {item};
    }
};

void test_custom_strategy_dispatch() {
    auto chunking = std::make_unique<MarkerChunkingStrategy>();
    auto retrieval = std::make_unique<MarkerRetrievalStrategy>();
    auto context = std::make_unique<MarkerContextStrategy>();

    aiws::ProcessingCore core(
        std::move(chunking),
        std::move(retrieval),
        std::move(context));

    aiws::Workspace workspace;
    workspace.add_document(
        aiws::Document("doc1", "Test Document", "ordinary document text"));

    core.rebuild(workspace);

    check(core.chunk_count() == 1,
          "custom chunking strategy should produce one chunk");

    if (core.chunk_count() == 1) {
        check(core.chunks()[0].text == "custom chunk",
              "ProcessingCore should use the injected chunking strategy");
    }

    const auto results = core.search("anything", 5);

    check(results.size() == 1,
          "custom retrieval strategy should produce one result");

    if (results.size() == 1) {
        check(results[0].chunk_id == "custom-result",
              "ProcessingCore should use the injected retrieval strategy");
    }

    const auto context_items = core.build_context("anything", 5, 10);

    check(context_items.size() == 1,
          "custom context strategy should produce one context item");

    if (context_items.size() == 1) {
        check(context_items[0].chunk_id == "custom-context",
              "ProcessingCore should use the injected context strategy");
    }
}

void test_null_strategies_rejected() {
    bool threw = false;

    try {
        aiws::ProcessingCore core(
            nullptr,
            std::make_unique<aiws::RetrievalEngine>(),
            std::make_unique<aiws::ContextBuilder>());
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    check(threw, "null chunking strategy should throw invalid_argument");

    threw = false;

    try {
        aiws::ProcessingCore core(
            std::make_unique<aiws::Chunker>(),
            nullptr,
            std::make_unique<aiws::ContextBuilder>());
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    check(threw, "null retrieval strategy should throw invalid_argument");

    threw = false;

    try {
        aiws::ProcessingCore core(
            std::make_unique<aiws::Chunker>(),
            std::make_unique<aiws::RetrievalEngine>(),
            nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    check(threw, "null context strategy should throw invalid_argument");
}

void test_move_only_processing_core() {
    static_assert(!std::is_copy_constructible_v<aiws::ProcessingCore>);
    static_assert(!std::is_copy_assignable_v<aiws::ProcessingCore>);
    static_assert(std::is_move_constructible_v<aiws::ProcessingCore>);
    static_assert(std::is_move_assignable_v<aiws::ProcessingCore>);

    aiws::ProcessingCore first;

    aiws::Workspace workspace;
    workspace.add_document(
        aiws::Document("move-doc", "Move Document", "alpha beta gamma"));

    first.rebuild(workspace);

    const auto original_count = first.chunk_count();

    aiws::ProcessingCore second(std::move(first));

    check(second.chunk_count() == original_count,
          "move construction should transfer ProcessingCore state");
}

void test_failed_rebuild_preserves_state() {
    aiws::ProcessingCore core;

    aiws::Workspace valid_workspace;
    valid_workspace.add_document(
        aiws::Document("original", "Original Document", "alpha beta gamma"));

    core.rebuild(valid_workspace);

    const auto original_count = core.chunk_count();

    aiws::Workspace invalid_workspace;
    invalid_workspace.add_document(
        aiws::Document("duplicate", "First Document", "first document"));
    invalid_workspace.add_document(
        aiws::Document("duplicate", "Second Document", "second document"));

    bool threw = false;

    try {
        core.rebuild(invalid_workspace);
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    check(threw, "duplicate document IDs should reject rebuild");

    check(core.chunk_count() == original_count,
          "failed rebuild should preserve the previous corpus");
}

void test_default_processing_core() {
    aiws::ProcessingCore core;

    aiws::Workspace workspace;
    workspace.add_document(
        aiws::Document("default-doc", "Default Document", "alpha beta gamma"));

    core.rebuild(workspace);

    check(core.chunk_count() > 0,
          "default ProcessingCore should use the M1 chunking implementation");

    const auto results = core.search("alpha", 5);

    check(!results.empty(),
          "default ProcessingCore should use the M1 retrieval implementation");

    const auto context = core.build_context("alpha", 5, 10);

    check(!context.empty(),
          "default ProcessingCore should use the M1 context implementation");
}

}  // namespace

int main() {
    test_custom_strategy_dispatch();
    test_null_strategies_rejected();
    test_move_only_processing_core();
    test_failed_rebuild_preserves_state();
    test_default_processing_core();

    if (failures != 0) {
        std::cerr << failures << " student test(s) failed\n";
        return 1;
    }

    std::cout << "All student tests passed\n";
    return 0;
}