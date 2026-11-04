#ifndef QCP_SPATIAL_INDEX_H
#define QCP_SPATIAL_INDEX_H

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

// Axis-aligned box in data coordinates. A side may be infinite: an item that is
// not bounded on an axis (an unbounded span, an anchored position) is indexed by
// the other axis only. Boxes are closed and well formed (lower <= upper).
struct QCPDataBox
{
    double keyLower;
    double keyUpper;
    double valueLower;
    double valueUpper;
};

// Packed R-tree with a sort-tile-recursive bulk load (Leutenegger, Edgington and
// López, ICDE 1997), the structure Skia and Chromium use to cull recorded draw
// operations. Static: build() replaces everything, queries never allocate.
class QCPSpatialIndex
{
public:
    void build(std::vector<std::pair<QCPDataBox, int>> entries);

    // Calls onHit(payload) for every entry whose box overlaps box (closed).
    template <typename F>
    void query(const QCPDataBox& box, F&& onHit) const
    {
        if (mNodes.empty())
            return;
        // One frame per level; a frame remembers how many children it has visited.
        struct Frame
        {
            std::uint32_t node;
            std::uint32_t next;
        };
        constexpr int kMaxDepth = 64;
        Frame stack[kMaxDepth];
        int top = 0;
        stack[0] = {mRoot, 0};
        while (top >= 0)
        {
            Frame& frame = stack[top];
            const Node& node = mNodes[frame.node];
            if (frame.next >= node.count)
            {
                --top;
                continue;
            }
            const std::uint32_t child = node.first + frame.next++;
            if (node.leaf)
            {
                const Entry& entry = mEntries[child];
                if (overlaps(entry.box, box))
                    onHit(entry.payload);
            }
            else if (const std::uint32_t childNode = mLinks[child];
                     overlaps(mNodes[childNode].box, box))
            {
                ++top;
                stack[top] = {childNode, 0};
            }
        }
    }

    [[nodiscard]] int size() const { return static_cast<int>(mEntries.size()); }

private:
    struct Entry
    {
        QCPDataBox box;
        int payload;
    };
    struct Node
    {
        QCPDataBox box;
        std::uint32_t first; // entry index in a leaf, link index otherwise
        std::uint32_t count;
        bool leaf;
    };

    static constexpr std::uint32_t kFanout = 16;

    static bool overlaps(const QCPDataBox& a, const QCPDataBox& b)
    {
        return a.keyLower <= b.keyUpper && b.keyLower <= a.keyUpper && a.valueLower <= b.valueUpper
            && b.valueLower <= a.valueUpper;
    }
    static bool hasNan(const QCPDataBox& box);
    static double centre(double lower, double upper);
    static double keyCentre(const QCPDataBox& box) { return centre(box.keyLower, box.keyUpper); }
    static double valueCentre(const QCPDataBox& box)
    {
        return centre(box.valueLower, box.valueUpper);
    }
    static QCPDataBox united(const QCPDataBox& a, const QCPDataBox& b);
    static QCPDataBox boxOf(const std::vector<Entry>& entries, std::uint32_t first,
                            std::uint32_t count);

    void packEntries();
    std::uint32_t addNode(QCPDataBox box, std::uint32_t first, std::uint32_t count, bool leaf);
    std::vector<std::uint32_t> packLevel(std::vector<std::uint32_t> level);

    std::vector<Entry> mEntries;
    std::vector<Node> mNodes;
    std::vector<std::uint32_t> mLinks;
    std::uint32_t mRoot = 0;
};

#endif // QCP_SPATIAL_INDEX_H
