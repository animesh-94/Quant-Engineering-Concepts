#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <map>
#include <list>
#include <iomanip>
#include <unordered_map>
#include <deque>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>

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

    struct Trade {
        int takerId;
        int makerId;
        double price;
        int quantity;
        Side takerSide;
    };

    // --- PUBLIC THREAD-SAFE INTERFACE ---

    void addOrder(const Order& order) {
        std::lock_guard<std::mutex> lock(bookMutex);
        if (order.getId() < 999) {
            cancelOrderInternal(999);
            cancelOrderInternal(1000);
        }
        addOrderInternal(order);
    }

    void cancelOrder(int id) {
        std::lock_guard<std::mutex> lock(bookMutex);
        cancelOrderInternal(id);
    }

    void runInternalAlgo() {
        std::lock_guard<std::mutex> lock(bookMutex);
        cancelOrderInternal(999);
        cancelOrderInternal(1000);

        double mid = getMidPriceInternal();
        double imbalance = getImbalanceInternal();
        if (mid == 0.0) return;

        double fairPrice = mid + ((imbalance - 0.5) * 0.40);
        double skew = netInventory * 0.10;
        double myBid = (fairPrice - 1.0) - skew;
        double myAsk = (fairPrice + 1.0) - skew;

        if (netInventory < maxInventory) 
            addOrderInternal(Order(999, OrderType::Limit, Side::Buy, myBid, 5));
        if (netInventory > -maxInventory) 
            addOrderInternal(Order(1000, OrderType::Limit, Side::Sell, myAsk, 5));
    }

    void printOrders() const {
        std::lock_guard<std::mutex> lock(bookMutex);
        std::cout << "\n--- CURRENT ORDER BOOK (Inventory: " << netInventory << ") ---" << std::endl;
        for (auto const& [price, list] : Asks) for (auto const& o : list) printOrderInternal(o);
        std::cout << "----------" << std::endl;
        for (auto const& [price, list] : Bids) for (auto const& o : list) printOrderInternal(o);
    }

    void printTradeTape() const {
        std::lock_guard<std::mutex> lock(bookMutex);
        std::cout << "\n--- TRADE TAPE ---" << std::endl;
        for (const auto& t : tradeTape) {
            std::cout << "Taker: " << t.takerId << " | Maker: " << t.makerId << " | Qty: " << t.quantity << " | Price: " << t.price << std::endl;
        }
    }

private:
    mutable std::mutex bookMutex;
    std::map<double, std::list<Order>, std::less<double>> Asks;
    std::map<double, std::list<Order>, std::greater<double>> Bids;
    std::unordered_map<int, OrderLocation> orderLookup;
    std::deque<Trade> tradeTape;

    int netInventory = 0;
    const int maxInventory = 100;

    // --- PRIVATE INTERNAL METHODS (NO LOCKS HERE) ---

    void addOrderInternal(const Order& order) {
        Order remainingOrder = matchAgainstOppositeInternal(order);
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
    }

    void cancelOrderInternal(int id) {
        auto it = orderLookup.find(id);
        if (it == orderLookup.end()) return;
        OrderLocation loc = it->second;
        if (loc.side == Side::Buy) {
            Bids[loc.price].erase(loc.it);
            if (Bids[loc.price].empty()) Bids.erase(loc.price);
        } else {
            Asks[loc.price].erase(loc.it);
            if (Asks[loc.price].empty()) Asks.erase(loc.price);
        }
        orderLookup.erase(id);
    }

    Order matchAgainstOppositeInternal(Order takerOrder) {
        // We use a template-like logic or simply branch because Bids/Asks are different types
        if (takerOrder.getSide() == Side::Buy) {
            return performMatching(takerOrder, Asks);
        } else {
            return performMatching(takerOrder, Bids);
        }
    }

    template<typename T>
    Order performMatching(Order takerOrder, T& oppositeMap) {
        while (!oppositeMap.empty() && takerOrder.getQuantity() > 0) {
            auto it = oppositeMap.begin();
            bool priceMatch = (takerOrder.getSide() == Side::Buy) 
                ? (it->first <= takerOrder.getPrice()) 
                : (it->first >= takerOrder.getPrice());
            
            if (takerOrder.getType() == OrderType::Market) priceMatch = true;

            if (priceMatch) {
                Order& maker = it->second.front();
                int qty = std::min(takerOrder.getQuantity(), maker.getQuantity());
                
                // Explicitly name the struct to fix the push_back error
                tradeTape.push_back(Trade{takerOrder.getId(), maker.getId(), it->first, qty, takerOrder.getSide()});

                if (maker.getId() == 999 || maker.getId() == 1000) {
                    if (maker.getSide() == Side::Buy) netInventory += qty;
                    else netInventory -= qty;
                }

                takerOrder.setQuantity(takerOrder.getQuantity() - qty);
                maker.setQuantity(maker.getQuantity() - qty);
                if (maker.getQuantity() == 0) {
                    orderLookup.erase(maker.getId());
                    it->second.pop_front();
                    if (it->second.empty()) oppositeMap.erase(it);
                }
            } else break;
        }
        return takerOrder;
    }

    double getMidPriceInternal() const {
        if (Bids.empty() || Asks.empty()) return 0.0;
        return (Bids.begin()->first + Asks.begin()->first) / 2.0;
    }

    double getImbalanceInternal() const {
        if (Bids.empty() || Asks.empty()) return 0.5;
        int b = 0, a = 0;
        for (const auto& o : Bids.begin()->second) b += o.getQuantity();
        for (const auto& o : Asks.begin()->second) a += o.getQuantity();
        return static_cast<double>(b) / (b + a);
    }

    void printOrderInternal(const Order& order) const {
        std::cout << "ID: " << std::setw(4) << order.getId()
                  << " | " << (order.getSide() == Side::Buy ? "BUY " : "SELL")
                  << " | Price: " << std::fixed << std::setprecision(2) << std::setw(8) << order.getPrice()
                  << " | Qty: " << order.getQuantity() << std::endl;
    }
};

void algoWorker(OrderBook& ob, std::atomic<bool>& stopSignal) {
    std::cout << "[ALGO THREAD] Started." << std::endl;
    while (!stopSignal) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        ob.runInternalAlgo();
    }
    std::cout << "[ALGO THREAD] Stopped." << std::endl;
}

int main() {
    OrderBook ob;
    std::atomic<bool> stopSignal(false);

    std::thread t(algoWorker, std::ref(ob), std::ref(stopSignal));

    ob.addOrder(OrderBook::Order(1, OrderBook::OrderType::Limit, OrderBook::Side::Sell, 105.0, 20));
    ob.addOrder(OrderBook::Order(2, OrderBook::OrderType::Limit, OrderBook::Side::Buy, 95.0, 20));

    std::this_thread::sleep_for(std::chrono::seconds(1));
    ob.printOrders();

    std::cout << "\n[MAIN] Market Taker Buy entering..." << std::endl;
    ob.addOrder(OrderBook::Order(3, OrderBook::OrderType::Market, OrderBook::Side::Buy, 0, 10));

    std::this_thread::sleep_for(std::chrono::seconds(1));
    ob.printOrders();
    ob.printTradeTape();

    stopSignal = true;
    t.join(); 

    return 0;
}
