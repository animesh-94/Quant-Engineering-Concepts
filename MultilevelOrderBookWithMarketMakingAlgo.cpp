#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <map>
#include <list>
#include <iomanip>
#include <unordered_map>

class OrderBook {
public:
    enum class OrderType { Market, Limit, GoodTillCanceled, FillOrKill_Limit };
    enum class Side { Buy, Sell };

    class Order {
    public:
        Order(int id, OrderType type, Side side, double price, int quantity)
            : id(id), type(type), side(side), price(price), quantity(quantity) {}

        int getId() const { return id; }
        OrderType getType() const { return type; }
        Side getSide() const { return side; }
        double getPrice() const { return price; }
        int getQuantity() const { return quantity; }
        void setQuantity(int new_quantity) { quantity = new_quantity; }

    private:
        int id;
        OrderType type;
        Side side;
        double price;
        int quantity;
    };

    struct OrderLocation {
        Side side;
        double price;
        std::list<Order>::iterator it;
    };

    Order matchAgainstOpposite(Order takerOrder) {
        if (takerOrder.getSide() == Side::Buy) {
            return processMatching(takerOrder, Asks);
        } else {
            return processMatching(takerOrder, Bids);
        }
    }

    template<typename T>
    Order processMatching(Order takerOrder, T& oppositeMap) {
        while (!oppositeMap.empty() && takerOrder.getQuantity() > 0) {
            auto it = oppositeMap.begin();
            double bestOppositePrice = it->first;
            auto& makerList = it->second;

            bool priceMatch = (takerOrder.getSide() == Side::Buy)
                ? (bestOppositePrice <= takerOrder.getPrice())
                : (bestOppositePrice >= takerOrder.getPrice());

            if (takerOrder.getType() == OrderType::Market) priceMatch = true;

            if (priceMatch) {
                Order& makerOrder = makerList.front();
                int tradeQty = std::min(takerOrder.getQuantity(), makerOrder.getQuantity());

                std::cout << "INSTANT MATCH ID: " << takerOrder.getId() << " with " << makerOrder.getId()
                          << " Qty " << tradeQty << " Price " << std::fixed << std::setprecision(2) << bestOppositePrice << std::endl;

                takerOrder.setQuantity(takerOrder.getQuantity() - tradeQty);
                makerOrder.setQuantity(makerOrder.getQuantity() - tradeQty);

                if (makerOrder.getId() == 999 || makerOrder.getId() == 1000) {
                    updateInventory(makerOrder.getSide(), tradeQty);
                }

                if (makerOrder.getQuantity() == 0) {
                    orderLookup.erase(makerOrder.getId());
                    makerList.pop_front();
                    if (makerList.empty()) oppositeMap.erase(it);
                }
            } else {
                break;
            }
        }
        return takerOrder;
    }

    void addOrder(const Order& order) {
        if (order.getId() < 999) {
            cancelOrder(999);
            cancelOrder(1000);
        }

        Order remainingOrder = matchAgainstOpposite(order);

        if (remainingOrder.getQuantity() > 0 && remainingOrder.getType() != OrderType::Market) {
            double price = remainingOrder.getPrice();
            if (remainingOrder.getSide() == Side::Buy) {
                Bids[price].push_back(remainingOrder);
                orderLookup[remainingOrder.getId()] = { Side::Buy, price, std::prev(Bids[price].end()) };
            } else {
                Asks[price].push_back(remainingOrder);
                orderLookup[remainingOrder.getId()] = { Side::Sell, price, std::prev(Asks[price].end()) };
            }
        }

        if (order.getId() < 999) {
            runInternalAlgo();
        }
    }

    void cancelOrder(int id) {
        auto lookupIt = orderLookup.find(id);
        if (lookupIt == orderLookup.end()) return;

        OrderLocation loc = lookupIt->second;
        if (loc.side == Side::Buy) {
            removeFromMap(id, loc, Bids);
        } else {
            removeFromMap(id, loc, Asks);
        }
    }

    template<typename T>
    void removeFromMap(int id, OrderLocation& loc, T& targetMap) {
        targetMap[loc.price].erase(loc.it);
        if (targetMap[loc.price].empty()) {
            targetMap.erase(loc.price);
        }
        orderLookup.erase(id);
        std::cout << "[SYSTEM] Order " << id << " successfully cancelled." << std::endl;
    }

    void updateInventory(Side side, int quantity) {
        if (side == Side::Buy) netInventory += quantity;
        else netInventory -= quantity;
    }

    double getMidPrice() const {
        if (Bids.empty() || Asks.empty()) return 0.0;
        return (Bids.begin()->first + Asks.begin()->first) / 2.0;
    }

    double getImbalance() const {
        if (Bids.empty() || Asks.empty()) return 0.5;
        int bidQty = 0;
        int askQty = 0;
        for (const auto& order : Bids.begin()->second) bidQty += order.getQuantity();
        for (const auto& order : Asks.begin()->second) askQty += order.getQuantity();
        return static_cast<double>(bidQty) / (bidQty + askQty);
    }

    void runInternalAlgo() {
        double mid = getMidPrice();
        double imbalance = getImbalance();
        if (mid == 0.0) return;

        // Shift fair price based on book imbalance (Predictive)
        double imbalanceShift = (imbalance - 0.5) * 0.40; 
        double fairPrice = mid + imbalanceShift;

        // Apply Inventory Skew (Risk Management)
        double skew = netInventory * 0.10;
        double myBidPrice = (fairPrice - 1.0) - skew;
        double myAskPrice = (fairPrice + 1.0) - skew;

        std::cout << "[ALGO] Imbalance: " << std::fixed << std::setprecision(2) << imbalance 
                  << " | Fair Price: " << fairPrice << " | Inventory: " << netInventory << std::endl;

        if (netInventory < maxInventory) {
            addOrder(Order(999, OrderType::Limit, Side::Buy, myBidPrice, 5));
        }
        if (netInventory > -maxInventory) {
            addOrder(Order(1000, OrderType::Limit, Side::Sell, myAskPrice, 5));
        }
    }

    void printOrders() const {
        std::cout << "\n--- ASKS ---" << std::endl;
        for (auto const& [price, list] : Asks) {
            for (auto const& o : list) printOrder(o);
        }
        std::cout << "--- BIDS ---" << std::endl;
        for (auto const& [price, list] : Bids) {
            for (auto const& o : list) printOrder(o);
        }
        std::cout << "----------------------------" << std::endl;
    }

private:
    std::map<double, std::list<Order>, std::less<double>> Asks;
    std::map<double, std::list<Order>, std::greater<double>> Bids;
    std::unordered_map<int, OrderLocation> orderLookup;

    int netInventory = 0;
    const int maxInventory = 100;

    void printOrder(const Order& order) const {
        std::cout << "ID: " << std::setw(4) << order.getId()
                  << " | " << (order.getSide() == Side::Buy ? "BUY " : "SELL")
                  << " | Price: " << std::fixed << std::setprecision(2) << std::setw(8) << order.getPrice()
                  << " | Qty: " << order.getQuantity() << std::endl;
    }
};

int main() {
    OrderBook orderBook;
    
    // Initial Market Orders
    orderBook.addOrder(OrderBook::Order(1, OrderBook::OrderType::Limit, OrderBook::Side::Sell, 105.0, 20));
    orderBook.addOrder(OrderBook::Order(2, OrderBook::OrderType::Limit, OrderBook::Side::Sell, 104.0, 10));
    orderBook.addOrder(OrderBook::Order(3, OrderBook::OrderType::Limit, OrderBook::Side::Buy, 96.0, 10));

    std::cout << "\n--- External Market Buy (ID 4) for 15 units arriving ---" << std::endl;
    orderBook.addOrder(OrderBook::Order(4, OrderBook::OrderType::Market, OrderBook::Side::Buy, 0, 15));

    orderBook.printOrders();
    return 0;
}
