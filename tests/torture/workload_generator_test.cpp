#include "workload_generator.hpp"

#include <cassert>

int main() {
    torture::WorkloadGenerator first(1337, 0, torture::WorkloadMode::All);
    torture::WorkloadGenerator replay(1337, 0, torture::WorkloadMode::All);
    torture::WorkloadGenerator other_worker(1337, 1, torture::WorkloadMode::All);
    std::string first_worker_key;
    std::string second_worker_key;
    int puts = 0;
    int gets = 0;
    int deletes = 0;
    int batches = 0;

    for (int i = 0; i < 1000; ++i) {
        const auto generated = first.next();
        const auto replayed = replay.next();
        assert(generated.type == replayed.type);
        assert(generated.key == replayed.key);
        assert(generated.value == replayed.value);
        assert(generated.batch.size() == replayed.batch.size());
        for (std::size_t j = 0; j < generated.batch.size(); ++j) {
            assert(generated.batch[j].type == replayed.batch[j].type);
            assert(generated.batch[j].key == replayed.batch[j].key);
            assert(generated.batch[j].value == replayed.batch[j].value);
        }
        assert(generated.batch.size() <= 500);
        if (!generated.key.empty()) {
            assert(generated.key.rfind("worker_0_", 0) == 0);
            if (first_worker_key.empty()) first_worker_key = generated.key;
        }
        for (const auto& operation : generated.batch) {
            assert(operation.key.rfind("worker_0_", 0) == 0);
            if (first_worker_key.empty()) first_worker_key = operation.key;
        }
        switch (generated.type) {
            case torture::GeneratedOperation::Type::Put: ++puts; break;
            case torture::GeneratedOperation::Type::Get: ++gets; break;
            case torture::GeneratedOperation::Type::Delete: ++deletes; break;
            case torture::GeneratedOperation::Type::Batch: ++batches; break;
        }
    }

    for (int i = 0; i < 1000 && second_worker_key.empty(); ++i) {
        const auto generated = other_worker.next();
        if (!generated.key.empty()) {
            second_worker_key = generated.key;
        } else if (!generated.batch.empty()) {
            second_worker_key = generated.batch.front().key;
        }
    }
    assert(!first_worker_key.empty() && !second_worker_key.empty());
    assert(first_worker_key.rfind("worker_0_", 0) == 0);
    assert(second_worker_key.rfind("worker_1_", 0) == 0);
    assert(puts > 0 && gets > 0 && deletes > 0 && batches > 0);

    torture::WorkloadGenerator batch_only(42, 3, torture::WorkloadMode::BatchOnly);
    for (int i = 0; i < 10; ++i) {
        const auto operation = batch_only.next();
        assert(operation.type == torture::GeneratedOperation::Type::Batch);
        assert(!operation.batch.empty() && operation.batch.size() <= 500);
    }
    return 0;
}
