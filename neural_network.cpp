
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>



double sigmoid(double x) {
    return 1.0 / (1.0 + exp(-x));
}

double sigmoid_deriv(double x) {
    double s = sigmoid(x);
    return s * (1.0 - s);
}



double rand_weight() {
    return ((double)rand() / RAND_MAX) * 2.0 - 1.0;
}


class NeuralNetwork {
public:

    int input_size;
    int hidden_size;
    int output_size;


    std::vector<std::vector<double>> W1; 
    std::vector<std::vector<double>> W2;
    std::vector<double>              B1;
    std::vector<double>              B2;

   
    std::vector<double> z1, a1;
    std::vector<double> z2, a2;

    double learning_rate;


    NeuralNetwork(int in, int hidden, int out, double lr = 0.5)
        : input_size(in), hidden_size(hidden), output_size(out),
          learning_rate(lr)
    {
        
        W1.assign(hidden_size, std::vector<double>(input_size));
        W2.assign(output_size, std::vector<double>(hidden_size));
        B1.assign(hidden_size, 0.0);
        B2.assign(output_size, 0.0);

        for (int i = 0; i < hidden_size; i++)
            for (int j = 0; j < input_size; j++)
                W1[i][j] = rand_weight();

        for (int i = 0; i < output_size; i++)
            for (int j = 0; j < hidden_size; j++)
                W2[i][j] = rand_weight();
    }

 
    std::vector<double> forward(const std::vector<double>& input) {
     
        z1.assign(hidden_size, 0.0);
        a1.assign(hidden_size, 0.0);
        for (int i = 0; i < hidden_size; i++) {
            for (int j = 0; j < input_size; j++)
                z1[i] += W1[i][j] * input[j];
            z1[i] += B1[i];
            a1[i] = sigmoid(z1[i]);
        }

     
        z2.assign(output_size, 0.0);
        a2.assign(output_size, 0.0);
        for (int i = 0; i < output_size; i++) {
            for (int j = 0; j < hidden_size; j++)
                z2[i] += W2[i][j] * a1[j];
            z2[i] += B2[i];
            a2[i] = sigmoid(z2[i]);
        }

        return a2;
    }

 
    void backward(const std::vector<double>& input,
                  const std::vector<double>& target)
    {
     
        std::vector<double> delta2(output_size);
        for (int i = 0; i < output_size; i++)
            delta2[i] = (a2[i] - target[i]) * sigmoid_deriv(z2[i]);

   
        std::vector<double> delta1(hidden_size, 0.0);
        for (int i = 0; i < hidden_size; i++) {
            for (int k = 0; k < output_size; k++)
                delta1[i] += W2[k][i] * delta2[k];
            delta1[i] *= sigmoid_deriv(z1[i]);
        }

        
        for (int i = 0; i < output_size; i++) {
            for (int j = 0; j < hidden_size; j++)
                W2[i][j] -= learning_rate * delta2[i] * a1[j];
            B2[i] -= learning_rate * delta2[i];
        }

     
        for (int i = 0; i < hidden_size; i++) {
            for (int j = 0; j < input_size; j++)
                W1[i][j] -= learning_rate * delta1[i] * input[j];
            B1[i] -= learning_rate * delta1[i];
        }
    }

    // ── Treinamento 
    void train(const std::vector<std::vector<double>>& X,
               const std::vector<std::vector<double>>& Y,
               int epochs)
    {
        for (int epoch = 0; epoch <= epochs; epoch++) {
            double loss = 0.0;
            for (size_t i = 0; i < X.size(); i++) {
                std::vector<double> pred = forward(X[i]);
                backward(X[i], Y[i]);
                double err = pred[0] - Y[i][0];
                loss += err * err;
            }
            loss /= X.size();

            if (epoch % 1000 == 0)
                std::cout << "Epoch " << std::setw(5) << epoch
                          << " | Loss: " << std::fixed
                          << std::setprecision(6) << loss << "\n";
        }
    }

   
    double predict(const std::vector<double>& input) {
        return forward(input)[0];
    }
};



int main() {
    srand((unsigned)time(nullptr));

    std::cout << "====================================\n";
    std::cout << "   Rede Neural C++ — Porta XOR\n";
    std::cout << "====================================\n\n";

  

    std::vector<std::vector<double>> X = {
        {0, 0}, {0, 1}, {1, 0}, {1, 1}
    };
    std::vector<std::vector<double>> Y = {
        {0}, {1}, {1}, {0}
    };


    NeuralNetwork nn(2, 4, 1, 0.5);

    std::cout << "Treinando por 10.000 epocas...\n\n";
    nn.train(X, Y, 10000);

    std::cout << "\n--- Resultados apos treinamento ---\n\n";
    std::cout << std::left
              << std::setw(10) << "Entrada"
              << std::setw(12) << "Esperado"
              << "Predito\n";
    std::cout << std::string(34, '-') << "\n";

    for (size_t i = 0; i < X.size(); i++) {
        double pred = nn.predict(X[i]);
        std::cout << "(" << X[i][0] << ", " << X[i][1] << ")   "
                  << std::setw(12) << Y[i][0]
                  << std::fixed << std::setprecision(4) << pred
                  << "  [" << (pred >= 0.5 ? "1" : "0") << "]\n";
    }

    std::cout << "\nRede treinada com sucesso!\n";
    return 0;
}
