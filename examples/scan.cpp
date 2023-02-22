#include <stdexec/execution.hpp>
#include <exec/any_sender_of.hpp>
#include <exec/static_thread_pool.hpp>

#include <span>
#include <numeric>
#include <vector>
#include <cassert>

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
  any_sender_of<set_value_t(T)> init_sender = init_sender__;

  while (!input.empty()) {
    std::size_t const N = std::min(input.size(), tile_count * block_size);

    sender auto first_phase = just(std::vector<T>{tile_count + 1})
        | bulk(tile_count,
            [=](std::size_t i, std::vector<T>& partials) {
              auto start = i * N / tile_count;
              auto end   = (i + 1) * N / tile_count;
              partials[i + 1] = *--std::inclusive_scan(std::begin(input) + start,
                                                        std::begin(input) + end,
                                                        std::begin(output) + start);
            });
    sender auto next_init_sender = when_all(init_sender, first_phase)
            | then([](T init, std::vector<T>&& partials) {
              partials[0] = init;
              std::inclusive_scan(std::begin(partials), std::end(partials),
                                  std::begin(partials));
              return std::move(partials);
            }) | bulk(tile_count,
            [=](std::size_t i, std::vector<T>& partials) {
              auto start = i * N / tile_count;
              auto end   = (i + 1) * N / tile_count;
              std::for_each(std::begin(output) + start, std::begin(output) + end,
                [a=partials[i]] (T& e) { e += a; }
              );
            }) | then([](std::vector<T>&& partials) {
                return partials.back();
              });
    init_sender = next_init_sender;
    input = input.subspan(N);
    output = output.subspan(N);
  }
  return init_sender;
}

int main(int argc, char *argv[]) {
  exec::static_thread_pool ctx{8};

  scheduler auto sch = ctx.get_scheduler();
  
  const std::size_t N = 100000;
  
  std::vector<std::size_t> a(N);
  std::iota(a.begin(), a.end(), 1);

  std::span a_span{a.begin(), a.end()};

  sender auto scan_back = async_inclusive_scan(just((std::size_t)0), a_span, a_span, 1000, 8);

  sender auto scan_back_on = on(sch, scan_back);

  auto back = this_thread::sync_wait(scan_back_on);

  assert(back.has_value() && std::get<0>(back.value()) == N * (N + 1) / 2);

  return 0;
}