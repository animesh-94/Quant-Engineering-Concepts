#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
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

    void addOrder(const Order& order) {
        orders.push_back(order);
    }

    void cancelOrder(int orderId) {
        auto it = std::remove_if(orders.begin(), orders.end(), [orderId](const Order& order) {
            return order.getId() == orderId;
        });

        if (it != orders.end()) {
            std::cout << "Canceled Order ID: " << orderId << std::endl;
            orders.erase(it, orders.end());
        }
    }

    void matchOrders() {
        // Pass 1: Handle Market orders
        for (auto it = orders.begin(); it != orders.end();) {
            if (it->getType() == OrderType::Market) {
                auto matchIt = findMatch(it, it->getQuantity());
                if (matchIt != orders.end()) {
                    executeOrder(it, matchIt);
                    it = orders.erase(it);
                } else {
                    ++it;
                }
            } else {
                ++it;
            }
        }

        // Pass 2: Handle GoodTillCanceled orders
        for (auto it = orders.begin(); it != orders.end();) {
            if (it->getType() == OrderType::GoodTillCanceled) {
                auto matchIt = findMatch(it, it->getQuantity());
                if (matchIt != orders.end()) {
                    executeOrder(it, matchIt);
                    it = orders.erase(it);
                } else {
                    ++it;
                }
            } else {
                ++it;
            }
        }

        // Pass 3: Handle Fill-Or-Kill orders
        for (auto it = orders.begin(); it != orders.end();) {
            if (it->getType() == OrderType::FillOrKill_Limit) {
                auto matchIt = findMatch(it, it->getQuantity(), true); 
                if (matchIt != orders.end() && matchIt->getQuantity() >= it->getQuantity()) {
                    executeOrder(it, matchIt);
                    it = orders.erase(it);
                } else {
                    std::cout << "Canceled FOK Order ID: " << it->getId() << std::endl;
                    it = orders.erase(it);
                }
            } else {
                ++it;
            }
        }

        // Pass 4: Handle remaining Limit orders
        for (auto it = orders.begin(); it != orders.end();) {
            if (it->getType() == OrderType::Limit) {
                auto matchIt = findMatch(it, it->getQuantity());
                if (matchIt != orders.end()) {
                    executeOrder(it, matchIt);
                    it = orders.erase(it);
                } else {
                    ++it;
                }
            } else {
                ++it;
            }
        }
    }

    void printOrders() const {
        for (const auto& order : orders) {
            printOrder(order);
        }
    }

private:
    std::vector<Order> orders;

    std::vector<Order>::iterator findMatch(std::vector<Order>::iterator orderIt, int quantity, bool fullMatch = false) {
        for (auto it = orders.begin(); it != orders.end(); ++it) {
            if (it == orderIt) continue; // Don't match with self
            
            if (it->getSide() != orderIt->getSide() &&
                ((orderIt->getSide() == Side::Buy && (orderIt->getType() == OrderType::Market || it->getPrice() <= orderIt->getPrice())) ||
                 (orderIt->getSide() == Side::Sell && (orderIt->getType() == OrderType::Market || it->getPrice() >= orderIt->getPrice()))) &&
                (!fullMatch || it->getQuantity() >= quantity)) {
                return it;
            }
        }
        return orders.end();
    }

    void executeOrder(std::vector<Order>::iterator& orderIt, std::vector<Order>::iterator& matchIt) {
        double fillPrice = matchIt->getPrice();
        int fillQty = std::min(orderIt->getQuantity(), matchIt->getQuantity());

        std::cout << "Matched Order ID: " << orderIt->getId() << " with Order ID: " << matchIt->getId() 
                  << " at Price: " << std::fixed << std::setprecision(2) << fillPrice 
                  << " quantity: " << fillQty << std::endl;

        matchIt->setQuantity(matchIt->getQuantity() - fillQty);
        if (matchIt->getQuantity() == 0) {
            orders.erase(matchIt);
        }
    }

    void printOrder(const Order& order) const {
        std::cout << "Order ID: " << order.getId() 
                  << ", Type: " << static_cast<int>(order.getType()) 
                  << ", Side: " << (order.getSide() == Side::Buy ? "Buy" : "Sell")
                  << ", Price: " << std::fixed << std::setprecision(2) << order.getPrice() 
                  << ", Quantity: " << order.getQuantity() << std::endl;
    }
};

int main() {
    OrderBook orderBook;

    // Use correct enums from class definition
    orderBook.addOrder(OrderBook::Order(1, OrderBook::OrderType::Market, OrderBook::Side::Buy, 0, 10));
    orderBook.addOrder(OrderBook::Order(2, OrderBook::OrderType::Limit, OrderBook::Side::Sell, 101.0, 20));
    orderBook.addOrder(OrderBook::Order(3, OrderBook::OrderType::Limit, OrderBook::Side::Sell, 99.0, 5));
    orderBook.addOrder(OrderBook::Order(4, OrderBook::OrderType::Market, OrderBook::Side::Sell, 0, 15));
    orderBook.addOrder(OrderBook::Order(5, OrderBook::OrderType::GoodTillCanceled, OrderBook::Side::Buy, 102.0, 10));
    orderBook.addOrder(OrderBook::Order(6, OrderBook::OrderType::FillOrKill_Limit, OrderBook::Side::Sell, 100.0, 8));
    orderBook.addOrder(OrderBook::Order(7, OrderBook::OrderType::FillOrKill_Limit, OrderBook::Side::Buy, 99.0, 12));
    orderBook.addOrder(OrderBook::Order(8, OrderBook::OrderType::FillOrKill_Limit, OrderBook::Side::Buy, 101.0, 8));

    std::cout << "Order Book before matching:" << std::endl;
    orderBook.printOrders();
    std::cout << "----------------------------" << std::endl;

    orderBook.matchOrders();

    std::cout << "----------------------------" << std::endl;
    std::cout << "Order Book after matching:" << std::endl;
    orderBook.printOrders();

    return 0;
}
