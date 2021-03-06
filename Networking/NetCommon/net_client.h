#pragma once
#include "net_common.h"
#include "net_message.h"
#include "net_tsqueue.h"
#include "net_connection.h"

namespace cap {
	namespace net {

		// Esta clase es responsable de establecer y configurar asio y la conexi�n,
		// tambi�n se comparte con un pointer para poder usar la aplicaci�n para comunicarse
		// con el server.

		template <typename T>
		class client_interface {
		private:

			// Este es el proceso seguro de la cola de subprocesos con mensajes entrantes del servidor.
			tsqueue<owned_message<T>> m_qMessagesIn;

		protected:

			// Los contextos de asio manejas las transferencias de datos.
			// Pero por cada contexto, este necesita su propio proceso para ejecutar sus comandos.

			asio::io_context m_context;
			std::thread thrContext;

			// El cliente tiene una �nica instancia de un objeto de "connection", el cual maneja la transferencia de datos.
			std::unique_ptr<connection<T>> m_connection;

		public:
			client_interface() {
			}

			virtual ~client_interface() {
				// Si el cliente es destruido, se desconectarse del server.
				this->Disconnect();
			}

			// Conecta al servidor con hostname o ip y con su puerto, retornara si la conexi�n fue posible.
			bool Connect(const std::string& host, const uint16_t port) {
				try {

					// Resuelve el hostname/ip en una direcci�n f�sica tangible.
					asio::ip::tcp::resolver resolver(this->m_context);

					// Creamos los puntos finales de la conexi�n y consigue el host y lo guarda en el punto final de la conexi�n.
					asio::ip::tcp::resolver::results_type m_endpoints = resolver.resolve(host, std::to_string(port));

					// Creando la conexi�n
						// Tenemos que especificarle que somos, el contexto que usamos y un socket con nuestro contexto.
						// Y tambi�n la cola de nuestros mensajes entrantes.
					this->m_connection = std::make_unique<connection<T>>(connection<T>::owner::client, this->m_context, asio::ip::tcp::socket(this->m_context), this->m_qMessagesIn); // TODO

					// Le indica a la conexi�n que se conecte al server.
					this->m_connection->ConnectToServer(m_endpoints);

					// Empieza un proceso con el contexto.
					this->thrContext = std::thread([this]() { m_context.run(); });

				}
				catch (std::exception& e) {
					// Imprimimos la excepci�n encontrada.
					std::cerr << "Excepci�n del Cliente: " << e.what() << "\n";
					return false;
				}

				return true;
			}

			// Desconecta del servidor.
			void Disconnect() {
				// Si la conexi�n existe, y esta conectada nos desconectamos.
				if (this->IsConnected()) {
					this->m_connection->Disconnect();
				}

				// Como acabamos con la conexi�n, tambi�n es necesario acabar con el contexto de asio y sus procesos.
				this->m_context.stop();
				if (this->thrContext.joinable()) {
					this->thrContext.join();
				}

				// Finalmente destruimos el objeto de la conexi�n.
				this->m_connection.release();
			}

			// Revisa si el cliente esta conectado al servidor.
			bool IsConnected() {
				if (this->m_connection) {
					return this->m_connection->IsConnected();
				}
				else {
					return false;
				}
			}
			
			// Manda un mensaje al servidor.
			void Send(const message<T>& msg) {
				// Verificamos que este conectado el cliente.
				if (this->IsConnected()) {
					this->m_connection->Send(msg);
				}
			}

			// Recupera la cola de mensajes del server.
			tsqueue<owned_message<T>>& Incoming() {
				return this->m_qMessagesIn;
			}

		};
	}
}