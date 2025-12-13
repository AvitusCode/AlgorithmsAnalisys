
namespace jd::memory
{
constexpr auto operator"" _KB(unsigned long long x) noexcept
{
    return x << 10;
}
constexpr auto operator"" _MB(unsigned long long x) noexcept
{
    return x << 20;
}

} // namespace jd::memory
