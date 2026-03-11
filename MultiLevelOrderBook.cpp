#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <map>
#include <list>
#include <iomanip>

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
                          << " Qty " << tradeQty << " Price " << bestOppositePrice << std::endl;
    
                takerOrder.setQuantity(takerOrder.getQuantity() - tradeQty);
                makerOrder.setQuantity(makerOrder.getQuantity() - tradeQty);
    
                if (makerOrder.getQuantity() == 0) {
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
        // Step 1: Try to match immediately (be a Taker)
        Order remainingOrder = matchAgainstOpposite(order);

        // Step 2: If quantity remains, be a Maker (except for Market orders)
        if (remainingOrder.getQuantity() > 0 && remainingOrder.getType() != OrderType::Market) {
            if (remainingOrder.getSide() == Side::Buy) {
                Bids[remainingOrder.getPrice()].push_back(remainingOrder);
            } else {
                Asks[remainingOrder.getPrice()].push_back(remainingOrder);
            }
        }
    }

    // Passively matches top of book (can be called for background matching)
    void matchOrders() {
        while (!Bids.empty() && !Asks.empty()) {
            auto bestBidIt = Bids.begin();
            auto bestAskIt = Asks.begin();

            if (bestBidIt->first >= bestAskIt->first) {
                auto& bidList = bestBidIt->second;
                auto& askList = bestAskIt->second;

                Order& buyOrder = bidList.front();
                Order& sellOrder = askList.front();

                int tradeQty = std::min(buyOrder.getQuantity(), sellOrder.getQuantity());

                std::cout << "PASSIVE MATCH: " << buyOrder.getId() << " vs " << sellOrder.getId() 
                          << " Qty: " << tradeQty << " Price: " << bestAskIt->first << std::endl;

                buyOrder.setQuantity(buyOrder.getQuantity() - tradeQty);
                sellOrder.setQuantity(sellOrder.getQuantity() - tradeQty);

                if (buyOrder.getQuantity() == 0) bidList.pop_front();
                if (sellOrder.getQuantity() == 0) askList.pop_front();

                if (bidList.empty()) Bids.erase(bestBidIt);
                if (askList.empty()) Asks.erase(bestAskIt);
            } else {
                break;
            }
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
    }

private:
    // Sorted Maps: Asks low-to-high, Bids high-to-low
    std::map<double, std::list<Order>, std::less<double>> Asks;
    std::map<double, std::list<Order>, std::greater<double>> Bids;

    void printOrder(const Order& order) const {
        std::cout << "ID: " << order.getId()
                  << " | " << (order.getSide() == Side::Buy ? "BUY" : "SELL")
                  << " | Price: " << std::fixed << std::setprecision(2) << order.getPrice()
                  << " | Qty: " << order.getQuantity() << std::endl;
    }
};

int main() {
    OrderBook orderBook;

    // Seeding the book with Makers (Limit Orders)
    orderBook.addOrder(OrderBook::Order(1, OrderBook::OrderType::Limit, OrderBook::Side::Sell, 101.0, 20));
    orderBook.addOrder(OrderBook::Order(2, OrderBook::OrderType::Limit, OrderBook::Side::Sell, 100.0, 10));
    
    std::cout << "Initial Book State:" << std::endl;
    orderBook.printOrders();

    // Crossing the spread (Taker Order)
    std::cout << "\nNew Market Buy Order (ID 3) for 15 units arriving..." << std::endl;
    orderBook.addOrder(OrderBook::Order(3, OrderBook::OrderType::Market, OrderBook::Side::Buy, 0, 15));

    std::cout << "\nFinal Book State:" << std::endl;
    orderBook.printOrders();

    return 0;
}
