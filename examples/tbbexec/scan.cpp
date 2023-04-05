#include <stdexec/execution.hpp>
#include <exec/any_sender_of.hpp>
#include <exec/single_thread_context.hpp>
#include <exec/static_thread_pool.hpp>
#include <tbbexec/tbb_thread_pool.hpp>

#include <span>
#include <numeric>
#include <vector>
#include <iostream>
#include <chrono>
#include <string>

class timer {
  const std::string name;
  const std::chrono::high_resolution_clock::time_point start;
  std::ostream &os;
public:
  timer(std::string s, std::ostream &_os = std::cerr) : name(s), start(std::chrono::high_resolution_clock::now()), os(_os) {
    os << name << " has started" << std::endl;
  }
  ~timer() {
    os << name << " took " << time() << "s" << std::endl;
  }
  double time() const {
    return std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - start).count();
  }
};

using namespace stdexec;
using namespace exec;

template <class... Ts>
using any_sender_of =
  typename any_receiver_ref<completion_signatures<Ts...>>::template any_sender<>;

template <typename T>
sender auto async_inclusive_scan(sender auto init_sender__,
                                 std::span<T> input,
                                 std::span<T> output,
                                 const std::size_t block_size,
                                 const std::size_t tile_count) {
  any_sender_of<set_value_t(T), set_stopped_t(), set_error_t(std::exception_ptr)> init_sender = std::move(init_sender__);

  while (!input.empty()) {
    std::size_t const N = std::min(input.size(), tile_count * block_size);

    sender auto first_phase = just(std::vector<T>(tile_count + 1))
        | bulk(tile_count,
            [=](std::size_t i, std::vector<T>& partials) noexcept {
              auto start = i * N / tile_count;
              auto end   = (i + 1) * N / tile_count;
              partials[i + 1] = *--std::inclusive_scan(std::begin(input) + start,
                                                        std::begin(input) + end,
                                                        std::begin(output) + start);
            });
    sender auto next_init_sender = when_all(std::move(init_sender), first_phase)
            | then([](T init, std::vector<T>&& partials) noexcept {
              partials[0] = init;
              std::inclusive_scan(std::begin(partials), std::end(partials),
                                  std::begin(partials));
              return std::move(partials);
            }) | bulk(tile_count,
            [=](std::size_t i, std::vector<T>& partials) noexcept {
              auto start = i * N / tile_count;
              auto end   = (i + 1) * N / tile_count;
              std::for_each(std::begin(output) + start, std::begin(output) + end,
                [a=partials[i]] (T& e) noexcept { e += a; }
              );
            }) | then([](std::vector<T>&& partials) noexcept {
                return partials.back();
              });
    init_sender = std::move(next_init_sender);
    input = input.subspan(N);
    output = output.subspan(N);
  }
  return init_sender;
}

int main(int argc, char *argv[]) {
  const int num_threads = 8;

  // exec::single_thread_context ctx;
  // exec::static_thread_pool ctx{num_threads};
  tbbexec::tbb_thread_pool ctx{num_threads};

  scheduler auto sch = ctx.get_scheduler();

  const std::size_t N = 500000000;

  std::vector<std::size_t> a(N);

  std::span a_span{a.begin(), a.end()};

  int sum = 0;

  for (int i = 0; i < 10; i++) {
    sender auto iota = just() | bulk(num_threads, [a_span](std::size_t i) {
      const auto start = i * a_span.size() / num_threads;
      const auto end = (i + 1) * a_span.size() / num_threads;
      std::iota(a_span.begin() + start, a_span.begin() + end, start);
    });

    sender auto iota_on = on(sch, std::move(iota));

    this_thread::sync_wait(std::move(iota_on));

    sender auto scan_back = async_inclusive_scan(just((std::size_t)0), a_span, a_span, 4096, num_threads);

    sender auto scan_back_on = on(sch, std::move(scan_back));

    timer t("scan");

    auto back = this_thread::sync_wait(std::move(scan_back_on));

    const auto [result] = *back;

    sum += result != N * (N - 1) / 2;
  }

  return sum;
}
