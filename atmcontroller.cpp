#include<bits/stdc++.h>
#include <poll.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
using namespace std;


class StdCinWithTimeout
{

    /*
    * Class to handle user input
    * called in separate async input thread 
    */	
    public:
    string read()
    {
    	string res;
    	getline(cin, res);
    	return res;
    }
};

class ATMDevice
{
    public:
    
    // returns the card number
    string readCard()
    {
        //It is interface for a real mechanical device 
        //I am using standard input
        string res;
        getline(cin, res); // blocking call to std input
        return res;
    }
    
    bool dispatchMoney(int amount)
    {
        //I will just return true from this virtual device, In reality it will call real device function 
        return true;
        
    } 
    
    
    bool receiveMoney(int amount)
    {
        //I will just return true from this virtual device, In reality it will call real device function 
        return true;
    }
};


class ATMBackendService
{
    /*
    * This client side object will make network communication and get data from servers
    * I am storing all the user data locally, I am using mutex/lock to illustrate that this data is shared
    * And this data should be accessed concurrently at server, Multiple request from diffrent ATM should be served concurrently by server
    * There should be lock to access user specific data if same user can access service through diffrent ATM
    * This object should be made at each ATM transaction and should store authentication token inorder to make subsequent call to server
    * I have made only one object of this to make simple illustration 
    */   
    unordered_map<string, size_t> user_auth_table; //card# , pin , card# are unique
    unordered_map<string, int> user_balance_table; //card#, balance
    
    std::mutex mtx;
    
    public:
    bool /*,token*/ authenticate(string card, size_t pin)
    {
        std::unique_lock<std::mutex> lck(mtx);
        if (user_auth_table.find(card) != user_auth_table.end())
        {
            if (user_auth_table[card] == pin)
            {
                return true;
            }
        }
        
        return false;
    }
    
    int getBalance(string card /*, token*/)
    {
        std::unique_lock<std::mutex> lck(mtx);
        if (user_balance_table.find(card) != user_balance_table.end())
        {
            return user_balance_table[card];
        }
        
        return -1; // to indicate error, balance can not be less than zero
    }
    
    bool deposit(string card, int amount /*, token*/)
    {
        std::unique_lock<std::mutex> lck(mtx);
        if (user_balance_table.find(card) != user_balance_table.end())
        {
            user_balance_table[card] += amount;
        }
        
        return true; // possible failure could include things like network failure in real system , i am returning true always
    }
    
    bool withdraw(string card, int amount /*, token*/)
    {
        std::unique_lock<std::mutex> lck(mtx);
        if (user_balance_table.find(card) != user_balance_table.end())
        {
            if (user_balance_table[card] - amount >= 0 )
            {
                user_balance_table[card] -= amount;
                return true;
            }
            else
            {
                return false;
            }
        }
        
        return false;
    }
    
    
    /*
    * Testing can be done at this object level and check if ledger is same as the legder at server side
    * Testing should also be done at the level of physical device, responsible to receiving in and dispatching money
    */
     
    // some data to interact with the system
    void setUserdata()
    {
        hash<string> hasher;
        
        user_auth_table["1111"] = hasher(string("1111"));
        user_auth_table["2222"] = hasher(string("2222"));
        user_auth_table["3333"] = hasher(string("3333"));
        user_auth_table["4444"] = hasher(string("4444"));
        user_auth_table["5555"] = hasher(string("5555"));
        
        user_balance_table["1111"] = 100;
        user_balance_table["2222"] = 0;
        user_balance_table["3333"] = 10;
        user_balance_table["4444"] = 50;
    }
    
    
};


class ATMUserInterface
{
    private:
    
    //user interface type
    enum class InterfaceType
    {
        Login,
        SimpleOneTransactionScreen,
        Message,
        Shutdown,
        ServiceDown
    };
    
    //type of message to decide appropriate message and user interface
    enum class MessageLevel
    {
        Info,
        TransactionError,
        ServiceError,
        SystemShutdown,
        Notdefined
    };
    
    //static constant strings used in application
    static const string serviceUnavailable;
    static const string ShuttingDown;
    static const string LoginWelcome;
    static const string SwipeYourCard;
    static const string CardDetail;
    static const string EnterPin;
    static const string PleaseWait;
    static const string LoginAuthorisationSuccess;
    static const string LoginAuthorisationFailure;
    static const string SelectTransaction;
    static const string _1_CheckBalance;
    static const string _2_Withdraw;
    static const string _3_Deposit;
    static const string EnterWithDrawAmount;
    static const string EnterDepositAmount;
    static const string Sorry;
    static const string BalanceSuccess;
    static const string WithdrawSuccess;
    static const string depositSuccess;
    static const string BalanceFailure;
    static const string WithdrawFailure;
    static const string DepositFailure;
    static const string InputTimeOut;
    static const string InvalidOption;
    
    //hidden state of object
    ATMBackendService* atmBackendService;
    ATMDevice* device;
    StdCinWithTimeout *t_cin;
    ATMUserInterface::InterfaceType cInterface;
    string card;
    string cmsg;
    ATMUserInterface::MessageLevel cmsgl;
    hash<string> stdhash;
    
    //shows login page and intract with user to move to other page
    //make async call for input and backend service while current thread handle user intraction
    void showLogin()
    {
        this->card = "";
        
        //I am using cout , but in reality it will be doen by GUI library
        cout << LoginWelcome << endl;
        cout << SwipeYourCard << endl;
        
        //Real GUI will also display different informational pictures (in motion)
        
        auto f1 = async(&ATMDevice::readCard, device); // get card detail from a physical reader device
        string card = f1.get();
        
        cout << CardDetail << card << endl;
        cout << EnterPin << endl;
        
        string pin;
        
        auto fi1 = async(&StdCinWithTimeout::read, t_cin); 
        std::chrono::system_clock::time_point ten_seconds_passed
          = std::chrono::system_clock::now() + std::chrono::seconds(10); 
        
        std::future_status status = fi1.wait_until(ten_seconds_passed);
        
        if (status == std::future_status::ready)
        {
            pin = fi1.get();
        }
        else
        {
            cout << InputTimeOut << endl;
            cout << LoginAuthorisationFailure << endl;
            cout << std::flush;
            return;
        }
        
        auto f2 = async(&ATMBackendService::authenticate, atmBackendService , card, stdhash(pin));
        //wait for backend to return
        cout << PleaseWait << endl;
        bool auth_success = f2.get();
        
        
        if (auth_success)
        {
            cout << LoginAuthorisationSuccess << endl;
            this->card = card;
            cInterface = InterfaceType::SimpleOneTransactionScreen;
        }
        else
        {
            cout << LoginAuthorisationFailure << endl;
        }
        
        //cInterface stay at InterfaceType::Login in case of failure
        cout << std::flush;
        
    }
    
    //Simple page to do one transaction at a time
    //make async call and move user to final message display page and allows for dispatch and receiving of money
    void showSimpleOneTransactionScreen()
    {
        cout << SelectTransaction << endl;
        cout << _1_CheckBalance << endl;
        cout << _2_Withdraw << endl;
        cout << _3_Deposit << endl;
        
        string res;
        
        auto fi1 = async(&StdCinWithTimeout::read, t_cin);
        std::chrono::system_clock::time_point ten_seconds_passed
          = std::chrono::system_clock::now() + std::chrono::seconds(10); 
        
        std::future_status status = fi1.wait_until(ten_seconds_passed);
        
        if (status == std::future_status::ready)
        {
            res = fi1.get();
            if (stoi(res) == 1)
            {
                auto f2 = async(&ATMBackendService::getBalance, atmBackendService , this->card);
                //wait for backend to return
                cout << PleaseWait << endl;
                int amount = f2.get();
                bool b_success =  amount >= 0 ? true : false;
                if (b_success)
                {
                    //go to bal Info page
                    cmsg = BalanceSuccess;
                    cmsg.append(to_string(amount));
                    cmsgl = MessageLevel::Info;
                    cInterface = InterfaceType::Message;
                }
                else
                {
                    //go to txn error page
                    cmsg = BalanceFailure;
                    cmsgl = MessageLevel::TransactionError;
                    cInterface = InterfaceType::Message;
                }
            }
            else if (stoi(res) == 2)
            {
                cout << EnterWithDrawAmount << endl;
                string res;
                
                auto fi1 = async(&StdCinWithTimeout::read, t_cin);
                std::chrono::system_clock::time_point ten_seconds_passed
          	  = std::chrono::system_clock::now() + std::chrono::seconds(10); 
                std::future_status status = fi1.wait_until(ten_seconds_passed);
                
                if (status == std::future_status::ready)
                {
                    res = fi1.get();
                    //Not doing validation of input amount
                    int amount = stoi(res);
                    auto f2 = async(&ATMBackendService::withdraw, atmBackendService , this->card, amount);
                    //wait for backend to return
                    cout << PleaseWait << endl;
                    bool w_success =  f2.get();
                    if (w_success)
                    {
                        auto f3 = async(&ATMDevice::dispatchMoney, device, amount); 
                        if (f3.get())
                        {
                            //goto success txn info page
                            cmsg = WithdrawSuccess;
                            cmsg.append(to_string(amount));
                            cmsgl = MessageLevel::Info;
                            cInterface = InterfaceType::Message;
                            
                        }
                        else
                        {
                            //goto txn device error page
                            cmsg = WithdrawFailure;
                            cmsgl = MessageLevel::TransactionError;
                            cInterface = InterfaceType::Message;
                        }
                    }
                    else
                    {
                        // got txn backend error page
                        cmsg = WithdrawFailure;
                        cmsgl = MessageLevel::TransactionError;
                        cInterface = InterfaceType::Message;
                    }
                }
                else
                {
                    //Go to time out error page 
                    cmsg = InputTimeOut;
                    cmsgl = MessageLevel::TransactionError;
                    cInterface = InterfaceType::Message;
                }
                
            }
            else if (stoi(res) == 3)
            {
                cout << EnterDepositAmount << endl;
                string res;
                
                auto fi1 = async(&StdCinWithTimeout::read, t_cin);
                std::chrono::system_clock::time_point ten_seconds_passed
          	  = std::chrono::system_clock::now() + std::chrono::seconds(10); 
                std::future_status status = fi1.wait_until(ten_seconds_passed);
      
                if (status == std::future_status::ready)
                {
                    res = fi1.get();
                    //Not doing validation of input amount
                    int amount = stoi(res);
                    auto f2 = async(&ATMBackendService::deposit, atmBackendService , this->card, amount);
                    //wait for backend to return
                    cout << PleaseWait << endl;
                    bool d_success =  f2.get();
                    if (d_success)
                    {
                        auto f3 = async(&ATMDevice::receiveMoney, device, amount); 
                        if (f3.get())
                        {
                            //goto success txn page
                            cmsg = depositSuccess;
                            cmsg.append(to_string(amount));
                            cmsgl = MessageLevel::Info;
                            cInterface = InterfaceType::Message;
                            
                        }
                        else
                        {
                            //got to device error page
                            cmsg = DepositFailure;
                            cmsgl = MessageLevel::TransactionError;
                            cInterface = InterfaceType::Message;
                        }
                        
                    }
                    else
                    {
                        // go to txn back error page
                        cmsg = DepositFailure;
                        cmsgl = MessageLevel::TransactionError;
                        cInterface = InterfaceType::Message;
                    }
                }
                else
                {
                    //Goto time out error page
                    cmsg = InputTimeOut;
                    cmsgl = MessageLevel::TransactionError;
                    cInterface = InterfaceType::Message;
                }
                
            
            }
            else
            {
                //Goto invalid option error page
                cmsg = InvalidOption;
                cmsgl = MessageLevel::TransactionError;
                cInterface = InterfaceType::Message;
                
            }
            
        }
        else
        {
            //Go time out error page
            cmsg = InputTimeOut;
            cmsgl = MessageLevel::TransactionError;
            cInterface = InterfaceType::Message;
        }
        this->card = "";
        cout << std::flush;
    }
    
    void showMessage()
    {
        //I am just printing using cout, but in reality it will be a separate page made with a GUI library
        switch(cmsgl)
        {
            case MessageLevel::Info:
                cout << cmsg << endl;
                break;
            case MessageLevel::TransactionError:
                cout << Sorry << cmsg << endl;
                break;
            case MessageLevel::ServiceError:
                cout << Sorry << cmsg <<endl;
                break;
            case MessageLevel::SystemShutdown:
                cout << ShuttingDown << endl;
                break;
            case MessageLevel::Notdefined:
            default:
                cout << "Undefined State" << endl;
                break;
            
        }
        cmsg.clear();
        cmsgl = MessageLevel::Notdefined;
        cout << std::flush;
    }
    
    public:
    
    ATMUserInterface(ATMBackendService *atmBackendService , ATMDevice* device, StdCinWithTimeout* t_cin):
    atmBackendService(atmBackendService),
    device(device),
    t_cin(t_cin),
    cInterface(InterfaceType::Login), 
    cmsg(""), 
    cmsgl(MessageLevel::Notdefined)
    {
        
    }
    
    void run()
    {
        // This is finite state machine that goes from one valid state to another valid state based on user interaction
        // using cInterface state variable ATM can be set to service down state or can turnoff remotely
        // there can be a reset state
        
        bool shutdown = false;
        while (!shutdown)
        {
            switch(cInterface)
            {
                case InterfaceType::Login:
                {
                    showLogin();
                    break; 
                }
                case InterfaceType::SimpleOneTransactionScreen:
                {
                    showSimpleOneTransactionScreen();
                    break;
                }
                case InterfaceType::Message:
                {
                    showMessage();
                    cInterface = InterfaceType::Login;
                    break;
                }
                case InterfaceType::Shutdown:
                {
                    cmsg = ATMUserInterface::ShuttingDown;
                    cmsgl = ATMUserInterface::MessageLevel::SystemShutdown;
                    showMessage();
                    shutdown = true;
                    break;
                }
                case InterfaceType::ServiceDown:
                default:
                {
                    cmsg = ATMUserInterface::serviceUnavailable;
                    cmsgl = ATMUserInterface::MessageLevel::ServiceError;
                    showMessage();
                    std::this_thread::sleep_for(std::chrono::seconds(30)); //30 seconds
                    // I have not implemented serviceDown or Shutdown state in detail, It is only for illustration
                    break;
                }
            }
        }
        
        //call to clean up and shutdown the machine
        
    }
};

//string definitions , can be done in multiple language
const string ATMUserInterface::serviceUnavailable = "ATM service is temporarily unavailable";
const string ATMUserInterface::ShuttingDown = "ATM is shutting down";
const string ATMUserInterface::LoginWelcome = "Welcome!";
const string ATMUserInterface::SwipeYourCard = "Please swipe or enter card number";
const string ATMUserInterface::CardDetail = "You card number is ";
const string ATMUserInterface::EnterPin = "Please enter your pin";
const string ATMUserInterface::PleaseWait = "Please wait for a moment..";
const string ATMUserInterface::LoginAuthorisationSuccess = "Success! Taking you to transaction page";
const string ATMUserInterface::LoginAuthorisationFailure = "Failed! Taking you to login page again";
const string ATMUserInterface::SelectTransaction = "Select an option";
const string ATMUserInterface::_1_CheckBalance = "1. Check Balance";
const string ATMUserInterface::_2_Withdraw = "2. Withdraw Money";
const string ATMUserInterface::_3_Deposit = "3. Deposit Money";
const string ATMUserInterface::EnterWithDrawAmount = "Enter withdraw amount";
const string ATMUserInterface::EnterDepositAmount = "Enter deposit amount";
const string ATMUserInterface::Sorry = "Sorry! ";
const string ATMUserInterface::BalanceSuccess = "Your Balance is ";
const string ATMUserInterface::WithdrawSuccess = "Withdrawal Amount ";
const string ATMUserInterface::depositSuccess = "Deposit Amount ";
const string ATMUserInterface::BalanceFailure = "Could not retreive your balance, Try again!";
const string ATMUserInterface::WithdrawFailure = "Could not withdraw the amount, if amount is less than your balance, then try again!";
const string ATMUserInterface::DepositFailure = "Could not deposit the amount, Try again!";
const string ATMUserInterface::InputTimeOut = "Please Provide the input in 10 seconds";
const string ATMUserInterface::InvalidOption = "Please choose one of the provided options";


// Driver code
int main()
{
    ATMBackendService atmBackendService;
    atmBackendService.setUserdata();
    ATMDevice device;
    StdCinWithTimeout t_cin;
    ATMUserInterface ui(&atmBackendService, &device, &t_cin);
    ui.run();
    
    return 0;
}
